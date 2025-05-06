#pragma once

#include "tbx/base/path.h"
#include "tbx/streams/file_data_source.h"

#include <memory>
#include <future>
#include <any>

using fs_path = path;

namespace flow
{  
  enum class InputMode { RealFile, Memory };

  struct Input
  {
  protected:
    size_t _size;

  public:
    Input() : _size(END_OF_STREAM) { }
    virtual ~Input() { }
    virtual void prepare(InputMode mode) { }
    virtual void finalize() { }

    virtual size_t size() const { return _size; }
    virtual size_t read(byte* dest, size_t amount) = 0;

    virtual const path& path() const = 0;
  };

  struct InputFile : public Input
  {
  protected:
    fs_path _path;
    std::unique_ptr<file_data_source> _source;

  public:
    InputFile(const fs_path& path) : _path(path) { }
    const fs_path& path() const { return _path; }

    void prepare(InputMode mode) override
    { 
      if (mode == InputMode::Memory)
      {
        _source.reset(new file_data_source(_path, false));
        _size = _source->size();
      }
      else
        /* do nothing */;
    }

    size_t read(byte* dest, size_t amount) override
    {
      return _source->read(dest, amount);
    }

    void finalize()
    {
      _source.reset();
    }
  };

  enum class OutputMode { RealFile, Memory };

  struct Output
  {
  public:
    virtual ~Output() { }
    virtual void prepare(OutputMode mode) {}
    virtual const path& path() const = 0;
  };

  struct OutputFile : public Output
  {
  protected:
    fs_path _path;
    std::unique_ptr<file_data_sink> _sink;

  public:
    OutputFile(const fs_path& path) : _path(path) { }
    const fs_path& path() const { return _path; }

    void prepare(OutputMode mode) override
    {
      if (mode == OutputMode::Memory) 
        _sink.reset(new file_data_sink(_path, false));
      else
        /* do nothing */;
    }
  };

  struct Value
  {
  protected:
    std::string _value;

  public:
    Value() { }

    void set(const std::string& value) { _value = value; }
    const std::string& get() const { return _value; }
  };

  struct CommandReporter
  {
    virtual ~CommandReporter() { }
    virtual void out(const std::string& message) = 0;
    virtual void err(const std::string& message) = 0;
    virtual void progress(float percent) = 0;
  };

  struct NullCommandReporter : public CommandReporter
  {
    void out(const std::string& message) override { }
    void err(const std::string& message) override { }
    void progress(float percent) override { }
  };

  struct ProgressLambdaReporter : public NullCommandReporter
  {
    std::function<void(float)> _progressCallback;
    ProgressLambdaReporter(std::function<void(float)> progressCallback) : _progressCallback(progressCallback) { }

    void progress(float percent) override
    {
      if (_progressCallback)
        _progressCallback(percent);
    }
  };


  using ident_t = std::string;
  using exit_code_t = int;


  template<typename T = std::string>
  struct Arg
  {
    ident_t name;
    T value;

    Arg(const ident_t& name, const T& value) : name(name), value(value) { }
  };

  struct Parameters
  {
    std::vector<Arg<Input*>> _inputs;
    std::vector<Arg<Output*>> _outputs;
    std::vector<Arg<>> _args;

    Parameters(Input* input, Output* output) 
    {
      if (input) addInput(input);
      if (output) addOutput(output);
    }

    Input* input() const;
    Output* output() const;

    const auto& inputs() const { return _inputs; }
    const auto& outputs() const { return _outputs; }

    void addInput(const ident_t& ident, Input* input) { _inputs.emplace_back(ident, input); }
    void addInput(Input* input) { addInput("", input); }

    void addOutput(const ident_t& ident, Output* output) { _outputs.emplace_back(ident, output); }
    void addOutput(Output* output) { addOutput("", output); }
  };

  struct CommandResult
  {
  protected:
    std::any _value;
    exit_code_t _exitCode;

  public:
    CommandResult(int exitCode) : _exitCode(exitCode) { }
    template<typename T> CommandResult(exit_code_t exitCode, T&& value) : _value(std::forward<T>(value)), _exitCode(exitCode) { }

    bool hasValue() const { return _value.has_value(); }
    exit_code_t exitCode() const { return _exitCode; }
  };

  struct Command
  {
    exit_code_t _exitCode;

  public:
    Command() : _exitCode(0) { }
    virtual ~Command() { }

    virtual int exitCode() const { return _exitCode; }

    /* run can be blocking so this must be taken account into */
    virtual void run(const Parameters& args, CommandReporter* reporter = nullptr) = 0;
    /* async execution, this must remain valid, args is copied */
    std::future<CommandResult> runAsync(const Parameters& args, CommandReporter* reporter = nullptr);
  };

  namespace commands
  {
    struct IsoToCso : public Command
    {
      void run(const Parameters& args, CommandReporter* reporter = nullptr) override;
    };

    struct InputToZip : public Command
    {
      void run(const Parameters& args, CommandReporter* reporter = nullptr) override;
    };
  }

}