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
  data_source* _source;
  data_sink* _sink;
  
  memory_buffer _buffer;

public:
  passthrough_pipe(data_source* source, data_sink* sink, size_t bufferSize) : _source(source), _sink(sink), _buffer(bufferSize)
  { }
  
  void stepInput()
  {
    /* available data to read is minimum between free room in buffer and remaining data */
    //size_t available = std::min(_bufferSize - _bufferPosition, _source->length() - _done);
    size_t available = _buffer.available();
    
    if (available > 0)
    {
      size_t effective = _source->read(_buffer.tail(), 1, available);
      _buffer.advance(effective);
    }
  }
  
  void stepOutput()
  {
    /* if there is data to process */
    if (!_buffer.empty())
    {
      size_t effective = _sink->write(_buffer.head(), 1, _buffer.used());
      
      /* TODO: circular buffer would be better? */
      /* we processed less data than total available, so we shift remaining */
      _buffer.consume(effective);
    }
  }
  
  void process() override
  {
    while (!_source->eos())
    {
      stepInput();
      stepOutput();
    }
  }
};

class data_mutator : public data_pipe
{
protected:
  data_source* _source;
  data_sink* _sink;
  
  memory_buffer _in, _out;

protected:
  virtual bool mutate() = 0;
  virtual void initialize() = 0;
  virtual void finalize() = 0;
  
public:
  data_mutator(data_source* source, data_sink* sink, size_t inBufferSize, size_t outBufferSize) :
  _source(source), _sink(sink), _in(inBufferSize), _out(outBufferSize) { }

  void stepInput()
  {
    if (!_in.full())
    {
      size_t effective = _source->read(_in.tail(), 1, _in.available());
      _in.advance(effective);
    }
  }
  
  void stepOutput()
  {
    if (!_out.empty())
    {
      size_t effective = _sink->write(_out.head(), 1, _out.used());
      _out.consume(effective);
    }
  }
  
  void process() override
  {
    initialize();
    
    while (!_source->eos())
    {
      stepInput();
      if (!mutate()) break;
      stepOutput();
    }
    
    while (mutate())
      stepOutput();
    
    finalize();
    
    while (!_out.empty())
      stepOutput();
  }
  
};





