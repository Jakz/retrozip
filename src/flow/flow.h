#pragma once

#include "tbx/base/path.h"

namespace flow
{
  struct Input
  {
  public:
    virtual ~Input() { }
    virtual void prepare() {}
  };

  struct InputFile : public Input
  {
  protected:
    path _path;

  public:
    InputFile(const path& path) : _path(path) { }
    const path& path() const { return _path; }
  };

  struct Output
  {
  public:
    virtual ~Output() { }
    virtual void prepare() {}
  };

  struct OutputFile : public Output
  {
  protected:
    path _path;

  public:
    OutputFile(const path& path) : _path(path) { }
    const path& path() const { return _path; }
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

  struct Args
  {
    // TODO
  };

  struct Command
  {
    int _exitCode;

  public:
    Command() : _exitCode(0) { }
    virtual ~Command() { }

    virtual int exitCode() const { return _exitCode; }

    /* run can be blocking so this must be taken account into */
    virtual void run(const Args& args, CommandReporter* reporter) = 0; 
  };

}