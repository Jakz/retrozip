#pragma once

#include "base/common.h"
#include "data_source.h"
#include "memory_buffer.h"

class data_pipe
{
  virtual void process() = 0;
};

class passthrough_pipe : public data_pipe
{
private:
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
  
  void stepInput()
  {
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
      }
      else
        _buffer.advance(effective);
    }
  }
  
  void stepOutput()
  {
    /* if there is data to process */
    if (!_buffer.empty())
    {
      size_t effective = _sink->write(_buffer.head(), _buffer.used());
      
      /* TODO: circular buffer would be better? */
      /* we processed less data than total available, so we shift remaining */
      _buffer.consume(effective);
      
      if (effective == END_OF_STREAM)
        _state = state::CLOSED;
    }
    else if (_buffer.empty() && _state == state::END_OF_INPUT)
    {
      size_t effective = _sink->write(nullptr, END_OF_STREAM);
      _state = effective != END_OF_STREAM ? state::NOTIFIED_SINK : state::CLOSED;
    }
  }
  
  void process() override
  {
    while (_state != state::CLOSED)
    {
      if (_state == state::OPENED)
        stepInput();
      
      if (_source->eos() && _state == state::OPENED)
      {
        //LOG("pipe::source::eos() state: OPENED -> END_OF_INPUT");
        _state = state::END_OF_INPUT;
      }
      
      stepOutput();
    }
  }
};



