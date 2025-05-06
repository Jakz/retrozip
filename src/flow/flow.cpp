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
  args.input()->prepare(InputMode::RealFile);
  args.output()->prepare(OutputMode::RealFile);
  
  fs_path inputPath = args.input()->path();
  fs_path outputPath = args.output()->path();
  
  subprocess::Popen proc(
    { "tools/maxcso/maxcso.exe", inputPath.str(), "-o", outputPath.str()},
    subprocess::output{ sp::PIPE },
    subprocess::error{ sp::PIPE });

  proc.communicate();
  _exitCode = proc.retcode();
}


#include "tbx/formats/minizip/zip.h"

void commands::InputToZip::run(const Parameters& args, CommandReporter* reporter)
{
  size_t bufferSize = MB1;
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[bufferSize]);

  Input* input = args.input();
  Output* output = args.output();

  input->prepare(InputMode::Memory);
  output->prepare(OutputMode::RealFile);

  size_t processed = 0;
  zipFile zip = zipOpen(output->path().c_str(), APPEND_STATUS_CREATE);
  if (zip)
  {
    zip_fileinfo zi = {};
    zipOpenNewFileInZip(zip, input->path().filename().c_str(), &zi, nullptr, 0, nullptr, 0, nullptr, Z_DEFLATED, Z_DEFAULT_COMPRESSION);

    size_t amount;

    while ((amount = input->read(buffer.get(), bufferSize)) != END_OF_STREAM)
    {
      processed += amount;
      reporter->progress(processed / float(input->size()));

      if (amount > 0)
        zipWriteInFileInZip(zip, buffer.get(), amount);
    }

    zipCloseFileInZip(zip);
    zipClose(zip, nullptr);
  }

  input->finalize();
}


