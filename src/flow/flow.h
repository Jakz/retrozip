#pragma once

#include "tbx/base/path.h"
#include "tbx/streams/file_data_source.h"

#include <memory>
#include <future>
#include <any>

using fs_path = path;

namespace flow
{  
  struct Input
  {
  public:
    virtual ~Input() { }
    virtual void prepare() { }
    virtual void finalize() { }

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

    void prepare() override { _source.reset(new file_data_source(_path, file_handle(_path, file_mode::READING, true))); }
  };

  struct Output
  {
  public:
    virtual ~Output() { }
    virtual void prepare() {}
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

    void prepare() override { _sink.reset(new file_data_sink(_path, false)); }
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

  struct Arg
  {
    // TODO
  };

  using ident_t = std::string;
  using exit_code_t = int;

  struct Parameters
  {
    std::unordered_map<ident_t, Input*> _inputs;
    std::unordered_map<ident_t, Output*> _outputs;
    std::vector<Arg> _args;

    Parameters(Input* input, Output* output) 
    {
      if (input) setInput(input);
      if (output) setOutput(output);
    }

    Input* input() const;
    Output* output() const;

    void setInput(const ident_t& ident, Input* input) { _inputs[ident] = input; }
    void setInput(Input* input) { _inputs[""] = input; }

    void setOutput(const ident_t& ident, Output* output) { _outputs[ident] = output; }
    void setOutput(Output* output) { _outputs[""] = output; }
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