#include "flow.h"

using namespace flow;

Parameters::Parameters(const std::string& original) : _original(original)
{
  /* simple argument tokenizer */
  std::string current;
  char quoted = '\0';

  for (size_t i = 0; i < original.size(); ++i)
  {
    char c = original[i];

    /* consume white space or add to current token if we're in quote */
    if (c == ' ')
    {
      if (quoted != '\0')
        current += ' ';
      else
      {
        if (!current.empty())
        {
          _tokens.push_back(current);
          current = std::string();
        }
        continue;
      }
    }
    else if (c == '\'' || c == '"')
    {
      if (quoted)
      {
        quoted = '\0';
        _tokens.push_back(current);
        current = std::string();
      }
      else
        quoted = c;
    }
    else
      current += c;
  }

  if (!current.empty())
    _tokens.push_back(current);
}

Input* Parameters::input() const
{
  if (_inputs.size() == 1)
    return _inputs.begin()->value;
  else
    return nullptr;
}

Output* Parameters::output() const
{
  if (_outputs.size() == 1)
    return _outputs.begin()->value;
  else
    return nullptr;
}


#include "tbx/extra/subprocess.hpp"
namespace sp = subprocess;

std::future<CommandResult> Command::runAsync(const Parameters& args, Engine* engine)
{
  return std::async(std::launch::async, [this, args, engine]() {
    auto result = this->run(args, engine);
    return result;
  });
}

CommandResult commands::IsoToCso::run(const Parameters& args, Engine* engine)
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

  return CommandResult(proc.retcode());
}


#include "tbx/formats/minizip/zip.h"
#include <numeric>

CommandResult commands::InputToZip::run(const Parameters& args, Engine* engine)
{
  size_t bufferSize = 1_mb;
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[bufferSize]);

  Output* output = args.output();

  output->prepare(OutputMode::RealFile);

  size_t processed = 0;
  size_t total = std::accumulate(args.inputs().begin(), args.inputs().end(), 0,
    [](size_t sum, const auto& pair) { return sum + pair.value->size(); });

  size_t i = 0;

  zipFile zip = zipOpen(output->path().c_str(), APPEND_STATUS_CREATE);
  if (zip)
  {
    for (const auto& arg : args.inputs())
    {
      Input* input = arg.value;
      input->prepare(InputMode::Memory);

      zip_fileinfo zi = {};
      zipOpenNewFileInZip(zip, input->path().filename().c_str(), &zi, nullptr, 0, nullptr, 0, nullptr, Z_DEFLATED, Z_DEFAULT_COMPRESSION);

      size_t amount;

      while ((amount = input->read(buffer.get(), bufferSize)) != END_OF_STREAM)
      {
        processed += amount;
        engine->reporter()->progress(((processed / float(input->size())) + i) * (1.0f / args.inputs().size()));

        if (amount > 0)
          zipWriteInFileInZip(zip, buffer.get(), amount);
      }

      zipCloseFileInZip(zip);
      input->finalize();

      processed = 0;
      ++i;
    }
    
    zipClose(zip, nullptr);
  }

  return CommandResult(0);
}


CommandResult commands::Echo::run(const Parameters& args, Engine* engine)
{
  if (args.token(0) == "echo")
  {
    engine->reporter()->out(args.original().substr(5));
    return CommandResult(0);
  }
  else
    return CommandResult();
}


namespace flow::commands
{
  struct Quit : public Command
  {
    CommandResult run(const Parameters& args, Engine* engine) override
    {
      if (args.token(0) == "exit")
      {
        engine->reporter()->out("Exiting...");
        engine->quit();
        return CommandResult(0);
      }
      else
        return CommandResult();
    }
  };

  struct Cd : public Command
  {
    CommandResult run(const Parameters& args, Engine* engine) override
    {
      if (args.token(0) == "cd" && args.tokenCount() == 2)
      {
        if (args.token(1) == "..")
        {
          auto& cwd = *engine->get<fs_path>("cwd");
          cwd = cwd.parent();
          engine->reporter()->out(fmt::format("Changed directory to: {}", cwd.str()));
        }
        else
        {
          fs_path path = args.token(1);

          if (path.isAbsolute())
            engine->set("cwd", path);
          else
          {
            fs_path cwd = *engine->get<fs_path>("cwd");
            fs_path potential = cwd + path;
            if (potential.exists())
            {
              engine->set("cwd", potential);
              engine->reporter()->out(fmt::format("Changed directory to: {}", potential.str()));
            }
            else
            {
              engine->reporter()->err(fmt::format("Directory does not exist: {}", potential.str()));
              return CommandResult(1);
            }
          }
        }

        return CommandResult(0, *engine->get<fs_path>("cwd"));
      }
      else
        return CommandResult();
    }
  };

  struct Ls : public Command
  {
    CommandResult run(const Parameters& args, Engine* engine) override
    {
      if (args.token(0) == "ls")
      {
        fs_path cwd = *engine->get<fs_path>("cwd");
        auto contents = cwd.contents();

        /* sort folders first */
        std::sort(contents.begin(), contents.end(), [](const fs_path& a, const fs_path& b) {
          return a.isFolder() && !b.isFolder();
          });

        for (const auto& entry : contents)
        {
          if (entry.isFolder())
            engine->reporter()->out(fmt::format("[color=#ffd86b]{}", entry.filename()));
          else
            engine->reporter()->out(fmt::format("[color=white]{}", entry.filename()));
        }

        return CommandResult(0);
      }
      else
        return CommandResult();
    }
  };

  struct Pwd : public Command
  {
    CommandResult run(const Parameters& args, Engine* engine) override
    {
      if (args.token(0) == "pwd")
      {
        fs_path cwd = *engine->get<fs_path>("cwd");
        engine->reporter()->out(fmt::format("Current directory: {}", cwd.str()));

        return CommandResult(0);
      }
      else
        return CommandResult();
    }
  };

}

#include "data/hash_map.h"
namespace flow::commands
{
  struct Md5 : public Command
  {
    CommandResult run(const Parameters& args, Engine* engine) override
    {
      if (args.token(0) == "md5" && args.tokenCount() == 2)
      {
        fs_path cwd = *engine->get<fs_path>("cwd");
        fs_path file = args.token(1);
        
        fs_path target = cwd / file;

        if (target.existsAsFile())
        {
          Hasher hasher;
          auto result = hasher.compute(target);
          engine->reporter()->out(fmt::format("{}", result.md5.literal()));
          return CommandResult(0);
        }
        else
        {
          engine->reporter()->err(fmt::format("File does not exist: {}", target.str()));
          return CommandResult(-1);
        }
      }
      else
        return CommandResult();
    }
  };

  struct LoadDat : public Command
  {
    CommandResult run(const Parameters& args, Engine* engine) override
    {
      if (args.token(0) == "load" && args.tokenCount() == 2)
      {
        fs_path cwd = *engine->get<fs_path>("cwd");
        fs_path file = args.token(1);

        fs_path target = cwd / file;

        if (target.existsAsFile())
        {
          engine->reporter()->out(fmt::format("Loading DAT file {}...", file.str()));
          return CommandResult(0);
        }
        else
        {
          engine->reporter()->err(fmt::format("File does not exist: {}", target.str()));
          return CommandResult(-1);
        }
      }
      else
        return CommandResult();
    }
  };
}

void Registry::init()
{
  //registerCommand(new commands::InputToZip());
  //registerCommand(new commands::IsoToCso());
  registerCommand(new commands::Echo());
  registerCommand(new commands::Quit());
  registerCommand(new commands::Cd());
  registerCommand(new commands::Ls());
  registerCommand(new commands::Pwd());
  registerCommand(new commands::Md5());
  registerCommand(new commands::LoadDat());
}

#include "cellar/database.h"

void Engine::init()
{
  _registry.init();
  _env.set("database", cellar::Database(nullptr, "database"));
  _env.set("cwd", fs_path::current());
}

void Engine::tryToExecute(const std::string& command)
{
  Parameters params(command);

  for (auto& cmd : _registry)
  {
    auto result = cmd->run(params, this);

    if (result.isRecognized())
    {
      if (result.hasValue())
      {
        _reporter->out(fmt::format(" : {} ({})", result.value().caption(), result.value().typeName() ));
      }
      
      return;
    }
  }

  _reporter->err(fmt::format("Command not recognized: {}", command));
}