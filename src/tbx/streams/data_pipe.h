#pragma once

#include "tbx/base/common.h"
#include "data_source.h"
#include "memory_buffer.h"

class data_pipe
{
  virtual void process() = 0;
};

class passthrough_pipe : public data_pipe
{
protected:
  enum class state
  {
    READY = 0,
    OPENED,
    END_OF_INPUT,
    NOTIFIED_SINK,
    CLOSED
  };
  
  data_source* _source;
  data_sink* _sink;
  
  memory_buffer _buffer;
  
  state _state;
  
public:
  passthrough_pipe(data_source* source, data_sink* sink, size_t bufferSize) : _source(source), _sink(sink), _buffer(bufferSize), _state(state::OPENED)
  { }
  
  size_t stepInput()
  {
    TRACE_P("%p: pipe::stepInput()", this);
    /* available data to read is minimum between free room in buffer and remaining data */
    //size_t available = std::min(_bufferSize - _bufferPosition, _source->length() - _done);
    size_t available = _buffer.available();
    
    if (available > 0)
    {
      size_t effective = _source->read(_buffer.tail(), available);
      
      if (effective == END_OF_STREAM)
      {
        assert(_state == state::OPENED);
        _state = state::END_OF_INPUT;
        TRACE_P("%p: pipe::stepInput() state: OPEN -> END_OF_INPUT", this);

      }
      else if (effective) //TODO: not necessary, used to skip tracing, just forward 0 in case
        _buffer.advance(effective);

      return effective;
    }

    return 0;
  }
  
  void stepOutput()
  {
    TRACE_P("%p: pipe::stepOutput()", this);
    
    /* if there is data to process */
    if (!_buffer.empty())
    {
      size_t effective = _sink->write(_buffer.head(), _buffer.used());
      
      /* TODO: circular buffer would be better? */
      /* we processed less data than total available, so we shift remaining */
      _buffer.consume(effective);
      
      if (effective == END_OF_STREAM && _state == state::NOTIFIED_SINK)
      {
        _state = state::CLOSED;
        TRACE_P("%p: pipe::stepOutput() state: NOTIFIED_SINK -> CLOSED", this);
      }
    }
    else if (_buffer.empty() && (_state == state::END_OF_INPUT || _state == state::NOTIFIED_SINK))
    {
      size_t effective = _sink->write(nullptr, END_OF_STREAM);
      
      if (effective != END_OF_STREAM && _state == state::END_OF_INPUT)
      {
        TRACE_P("%p: pipe::stepOutput() state: END_OF_INPUT -> NOTIFIED_SINK", this);
        _state = state::NOTIFIED_SINK;
      }
      else if (effective == END_OF_STREAM && (_state == state::NOTIFIED_SINK || _state == state::END_OF_INPUT))
      {
        TRACE_P("%p: pipe::stepOutput() state: NOTIFIED_SINK -> CLOSED", this);
        _state = state::CLOSED;
      }
    }
  }
  
  inline size_t step()
  {
    size_t effective = 0;
    
    if (_state == state::OPENED)
      effective = stepInput();
    
    stepOutput();

    return 0;
  }
  
  void process() override
  {
    while (_state != state::CLOSED)
      step();
    
    TRACE_P("%p: pipe::process() pipe closed", this);
  }
  
  void process(size_t requiredSize)
  {
    size_t size = 0;
    
    while (_state != state::CLOSED)
    {
      if (_state == state::OPENED)
        stepInput();
      
      size_t availableOutput = _buffer.used();
      
      stepOutput();
      
      size += availableOutput - _buffer.used();
      
      if (size >= requiredSize)
        break;
    }
    
    TRACE_P("%p: pipe::process() pipe closed", this);
  }
  
  void process(std::function<void(void)> monitor)
  {    
    while (_state != state::CLOSED)
    {
      step();
      monitor();
    }
    
    TRACE_P("%p: pipe::process() pipe closed", this);
  }
};

class observable_passthrough_pipe : public passthrough_pipe
{
protected:
  std::function<void(size_t)> _monitor;

public:
  observable_passthrough_pipe(data_source* source, data_sink* sink, size_t bufferSize, const std::function<void(size_t)>& monitor) :
    passthrough_pipe(source, sink, bufferSize), _monitor(monitor)
  { }

  void process(size_t requiredSize)
  {
    size_t size = 0;

    while (_state != state::CLOSED)
    {
      if (_state == state::OPENED)
        stepInput();

      size_t availableOutput = _buffer.used();
     
      stepOutput();

      size += availableOutput - _buffer.used();
      _monitor(size);

      if (size >= requiredSize)
        break;
    }

    TRACE_P("%p: pipe::process() pipe closed", this);
  }
};

class process_task
{
protected:
  std::string _ident;

public:
  process_task(const std::string& ident) : _ident(ident) { }

  virtual void prepare() { }
  virtual void finalize() { }

  virtual size_t size() const { return 0; }
  virtual data_source* source() const { return nullptr; }
  virtual data_sink* sink() const { return nullptr; }

  void execute(size_t bufferPolicy, const std::function<void(float)>& monitor);
};

class simple_process_task : public process_task
{
protected:
  data_source* _source;
  data_sink* _sink;
  size_t _size;

public:
  simple_process_task(const std::string& ident, data_source* source, data_sink* sink, size_t size) :
    process_task(ident), _source(source), _sink(sink), _size(size)
  { }

  data_source* source() const override { return _source; }
  data_sink* sink() const override { return _sink; }
  size_t size() const override { return _size; }
};

class process_task_list 
{
protected:
  std::vector<std::unique_ptr<process_task>> _tasks;

public:
  process_task_list() { }
  ~process_task_list() { }
  process_task_list(const process_task_list&) = delete;
  process_task_list(process_task_list&&) = default;
  process_task_list& operator=(const process_task_list&) = delete;
  process_task_list& operator=(process_task_list&&) = default;
  

  void add(process_task* task)
  {
    _tasks.push_back({});
    _tasks.back().reset(task);
  }

  auto begin() const { return _tasks.begin(); }
  auto end() const { return _tasks.end(); }
};
