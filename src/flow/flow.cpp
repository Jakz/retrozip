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

void commands::IsoToCso::run(const Parameters& args, CommandReporter* reporter)
{
  fs_path inputPath = args.input()->path();
  fs_path outputPath = args.output()->path();
  
  subprocess::Popen proc(
    { "tools/maxcso/maxcso.exe", inputPath.str(), "-o", outputPath.str()},
    subprocess::output{ sp::PIPE },
    subprocess::error{ sp::PIPE });

  proc.communicate();
}