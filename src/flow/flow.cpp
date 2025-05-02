#include "flow.h"

using namespace flow;

Input* Parameters::input() const
{
  if (_inputs.size() == 1)
    return _inputs.begin()->second;
  else
    return nullptr;
}

Output* Parameters::output() const
{
  if (_outputs.size() == 1)
    return _outputs.begin()->second;
  else
    return nullptr;
}


#include "tbx/extra/subprocess.hpp"
namespace sp = subprocess;

std::future<CommandResult> Command::runAsync(const Parameters& args, CommandReporter* reporter)
{
  return std::async(std::launch::async, [this, args, reporter] {
    this->run(args, reporter);
    return CommandResult(this->exitCode(), std::any());
  });
}

void commands::IsoToCso::run(const Parameters& args, CommandReporter* reporter)
{
  fs_path inputPath = args.input()->path();
  fs_path outputPath = args.output()->path();
  
  subprocess::Popen proc(
    { "tools/maxcso/maxcso.exe", inputPath.str(), "-o", outputPath.str()},
    subprocess::output{ sp::PIPE },
    subprocess::error{ sp::PIPE });

  proc.communicate();
  _exitCode = proc.retcode();
}

void commands::InputToZip::run(const Parameters& args, CommandReporter* reporter)
{
  size_t bufferSize = MB1;
  Input* input = args.input();

  input->prepare();
}


