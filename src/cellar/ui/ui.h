#pragma once

#include "cellar/kernel.h"

#include <string>
#include <vector>

struct GLFWwindow;

struct DatFile;
struct Game;
struct DatRom;

namespace meta
{
  class System;
}

namespace cellar
{
  class UserInterface : public KernelModule
  {
  protected:
    GLFWwindow* _window = nullptr;
    std::vector<std::string> _consoleMessages;
    std::string _selectedTitle = "Repository";
    std::string _selectedDetails;
    bool _scrollConsoleToBottom = false;

    bool createWindow();
    void destroyWindow();
    void configureStyle();
    void drawFrame();
    void drawToolbar();
    void drawRepositoryBrowser();
    void drawSystemNode(const meta::System& system);
    void drawDatNode(const DatFile& dat);
    void drawGameNode(const Game& game);
    void drawRomLeaf(const DatRom& rom);
    void drawInspector();
    void drawConsole();
    void selectRepository();
    void selectSystem(const meta::System& system);
    void selectDat(const DatFile& dat);
    void selectGame(const Game& game);
    void selectRom(const DatRom& rom);

  public:
    UserInterface(Kernel* kernel, const std::string& name);
    ~UserInterface();

    void init();

    void appendConsoleMessage(const std::string& message);
  };
}
