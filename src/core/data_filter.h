#pragma once

class unbuffered_data_filter
{
protected:
  virtual void process(const byte* data, size_t amount, size_t effective) = 0;
};

class lambda_unbuffered_data_filter : unbuffered_data_filter
{
public:
  using lambda_t = std::function<void(const byte*, size_t, size_t)>;
  lambda_unbuffered_data_filter(lambda_t lambda) : _lambda(lambda) { }
private:
  lambda_t _lambda;
protected:
  void process(const byte* data, size_t amount, size_t effective) override final { _lambda(data, amount, effective); }
};

template<typename T>
class unbuffered_source_filter : public data_source
{
protected:
  data_source* _source;
  T _filter;
public:
  template<typename... Args>
  unbuffered_source_filter(data_source* source, Args... args) : _source(source), _filter(args...) { }
  
  size_t read(byte* dest, size_t amount) override
  {
    size_t read = _source->read(dest, amount);
    _filter.process(dest, amount, read);
    return read;
  }
  
  T& filter() { return _filter; }
  const T& filter() const { return _filter; }
};

template<typename T>
class unbuffered_sink_filter : public data_sink
{
protected:
  data_sink* _sink;
  T _filter;
public:
  template<typename... Args>
  unbuffered_sink_filter(data_sink* sink, Args... args) : _sink(sink), _filter(args...) { }
  
  size_t write(const byte* src, size_t amount) override
  {
    size_t written = _sink->write(src, amount);
    _filter.process(src, amount, written);
    return written;
  }
  
  T& filter() { return _filter; }
  const T& filter() const { return _filter; }
};
