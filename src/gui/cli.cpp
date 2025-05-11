#include <cstdio>


#include "box/archive.h"
#include "tbx/streams/file_data_source.h"
#include "box/archive_builder.h"

#include "tbx/extra/subprocess.hpp"

#include "flow/flow.h"
#include "BearLibTerminal.h"

flow::Engine f;

class Terminal : public flow::CommandReporter
{
protected:
  std::string _prompt;
  int32_t _caretPosition;
  std::vector<std::string> _history;
  std::vector<std::string> _buffer;
    
  bool _shouldQuit;

  int _width, _height;

  static constexpr int _blinkInterval = 500;
  bool _caretVisible;
  std::chrono::steady_clock::time_point _lastBlink;

  void onResize();

public:

  Terminal() : _caretVisible(true), _shouldQuit(false), _caretPosition(0)
  {
    _width = 80;
    _height = 25;
    _prompt = "";
  }

  void out(const std::string& message) override { _buffer.push_back(message); }
  void err(const std::string& message) override { _buffer.push_back("[color=red]" + message + "[color=white]"); }
  void progress(float percent) override { }

  void init();
  void deinit();

  void loop();
  void render();

};

void Terminal::onResize()
{
  _width = terminal_state(TK_WIDTH);
  _height = terminal_state(TK_HEIGHT);
}

void Terminal::loop()
{
  /* init caret management */
  _caretVisible = true;
  _lastBlink = std::chrono::steady_clock::now();

  static std::array<char, 256> mapping = { { '\0' }};
  for (int i = TK_A; i <= TK_Z; ++i)
    mapping[i] = i - TK_A + 'a';
  for (int i = TK_1; i <= TK_9; ++i)
    mapping[i] = i - TK_1 + '1';
  mapping[TK_0] = '0';
  mapping[TK_MINUS] = '-';
  mapping[TK_COMMA] = ',';
  mapping[TK_PERIOD] = '.';
  mapping[TK_SLASH] = '/';
  mapping[TK_SPACE] = ' ';

  while (!_shouldQuit)
  {
    if (f.shouldQuit())
      _shouldQuit = true;
    
    auto now = std::chrono::steady_clock::now();
    auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(now - _lastBlink);
    if (delta.count() > _blinkInterval)
    {
      _caretVisible = !_caretVisible;
      _lastBlink = std::chrono::steady_clock::now();
    }

    while (terminal_has_input() && !_shouldQuit)
    {
      int key = terminal_read();

      if (key == TK_CLOSE || key == TK_ESCAPE)
        _shouldQuit = true;
      else if (key == TK_RESIZED)
        onResize();

      if (key == TK_ENTER)
      {
        /* execute command */
        f.tryToExecute(_prompt);
        _prompt.clear();
      }
      else if (key == TK_LEFT)
      {
        if (_caretPosition > -(int)_prompt.size())
          --_caretPosition;
      }
      else if (key == TK_RIGHT)
      {
        if (_caretPosition < 0)
          ++_caretPosition;
      }
      else if (key == TK_DELETE)
      {
        if (_caretPosition < 0)
        {
          _prompt.erase(_prompt.end() + _caretPosition);
          ++_caretPosition;
        }
      }
      else if (key == TK_BACKSPACE)
      {
        if (_caretPosition > -(int)_prompt.size())
          _prompt.erase(_prompt.end() + _caretPosition - 1);
      }
      else if (key == TK_HOME)
      {
        _caretPosition = 0;
      }
      else if (key == TK_END)
      {
        _caretPosition = static_cast<int>(_prompt.size());
      }
      else if (key == TK_BACKSPACE)
      {
        if (!_prompt.empty())
          _prompt.pop_back();
      }
      else if (mapping[key] != '\0')
        _prompt.insert(_prompt.end() + _caretPosition, mapping[key]);
    }

    render();
    terminal_delay(16); // ~60 FPS
  }

  deinit();
}

void Terminal::render()
{
  terminal_clear();

  for (size_t i = 0; i < _buffer.size(); ++i)
  {
    terminal_print(0, i, _buffer[i].c_str());
  }

  terminal_printf(0, _height - 2, ">%s", _prompt.c_str());
  terminal_layer(1);
  if (_caretVisible)
    terminal_put(1 + static_cast<int>(_prompt.size()) + _caretPosition, _height - 2, '_');
  terminal_layer(0);

  terminal_refresh();
}

void Terminal::init()
{  
  if (!terminal_open())
    assert(false);

  terminal_set("window: title='RetroZip', resizeable=true, size=80x25, cellsize=8x16");
  
  terminal_set("font: consola.ttf, size=12");

  // terminal_set("input: cursor-symbol=0x1f, cursor-blink-rate=500");

  terminal_set("0xE000: flags.png, size=16x16, align=center, spacing=2x1");

  terminal_refresh();
  onResize();

  f.setReporter(this);
}

void Terminal::deinit()
{
  terminal_close();
}

int main(int argc, char* argv[])
{
  Terminal terminal;
  terminal.init();
  terminal.loop();
  return 0;
}


#include <thread>

#include "flow/flow.h"

int maindisabled(int argc, char* argv[])
{
  if (false)
  {
    ArchiveBuilder builder(CachePolicy(CachePolicy::Mode::NEVER, 0), MB128, MB128);
    std::vector<FileGroup> groups;

    groups.push_back(FileGroup::ofSolid(path(R"(C:\Users\Jack\Desktop\patapon\smb3)").contents()));
    groups.push_back(FileGroup::ofBaseWithDelta(path(R"(C:\Users\Jack\Desktop\patapon\files)").contents(), 0));
    
    auto archive = builder.build(groups);
    memory_buffer sink;
    archive.options().bufferSize = MB64;
    archive.write(sink);
    sink.serialize(file_handle(R"(C:\Users\Jack\Desktop\patapon\whole.box)", file_mode::WRITING));
  }

  if (false)
  {
    ArchiveBuilder builder(CachePolicy(CachePolicy::Mode::NEVER, 0), MB128, MB128);

    auto sources = builder.buildSourcesFromFolder(R"(C:\Users\Jack\Desktop\patapon\smb3)");
    auto archive = builder.buildSingleStreamSolidArchive(sources);
    memory_buffer sink;
    archive.options().bufferSize = MB1;
    archive.write(sink);
    sink.serialize(file_handle(R"(C:\Users\Jack\Desktop\patapon\smb3.box)", file_mode::WRITING));
  }

  

  //loadFile(R"(C:\Users\Jack\Documents\dev\retrozip\projects\msvc2017\cellar\vault\f0\f071d45d8f5cb05b48d7d2b804c6cb6a79ad96fb.box)");

  flow::Registry registry;
  registry.init();

  flow::Parameters test = flow::Parameters("compress zip 'C:/Users/Jack/Desktop/patapon/files/1379 - Patapon (USA).iso)' 'C:/Users/Jack/Desktop/patapon/output.zip'");

  flow::InputFile input(R"(C:/Users/Jack/Desktop/patapon/files/1379 - Patapon (USA).iso)");
  flow::InputFile input2(R"(C:/Users/Jack/Desktop/patapon/files/1364 - Patapon (Europe) (En,Fr,De,Es,It).iso)");

  flow::OutputFile output(R"(C:/Users/Jack/Desktop/patapon/output.cso)");
  flow::OutputFile outputZip(R"(C:/Users/Jack/Desktop/patapon/output.zip)");
  flow::Parameters params(&input, &outputZip);
  params.addInput(&input2);
  
  flow::commands::InputToZip command;

  flow::ProgressLambdaReporter reporter([](float percent) {
    int width = 20;
    int pos = static_cast<int>(width * percent);
    std::cout << "\r[";
    for (int i = 0; i < width; ++i)
      std::cout << (i < pos ? "=" : " ");
    std::cout << "] " << std::fixed << std::setprecision(1) << (percent * 100) << "%";
    std::cout.flush();
  });

  auto result = command.runAsync(params, &f);
  result.wait_for(std::chrono::seconds(30));

  if (false)
  {
    namespace sp = subprocess;

    //auto process = sp::Popen({ "tools/maxcso/maxcso.exe", "C:/Users/Jack/Desktop/patapon/files/1379 - Patapon (USA).iso", "-o", "C:/Users/Jack/Desktop/patapon/output.cso"}, sp::output{sp::PIPE}, sp::error{sp::PIPE});
    //auto result = process.communicate();

    std::future<std::pair<sp::OutBuffer, sp::ErrBuffer>> future = std::async(std::launch::async, [] {
      subprocess::Popen proc(
        { "tools/maxcso/maxcso.exe", "C:/Users/Jack/Desktop/patapon/files/1379 - Patapon (USA).iso", "-o", "C:/Users/Jack/Desktop/patapon/output.cso" },
        subprocess::output{ sp::PIPE },
        subprocess::error{ sp::PIPE });
      return proc.communicate();
      });

    while (future.wait_for(std::chrono::milliseconds(100)) != std::future_status::ready) {
      std::cout << "Still working...\n";
    }

    // When done:
    auto [stdout_str, stderr_str] = future.get();
  }

  //std::cout << "Data : " << std::string(&result.second.buf[0]) << std::endl;

  return 0;
}