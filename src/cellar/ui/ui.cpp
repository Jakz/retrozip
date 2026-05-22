#include "ui.h"

#include "cellar/database.h"
#include "cellar/fs/cellar_fs.h"
#include "data/meta.h"
#include "tbx/base/strings.h"
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <iostream>
#include <unordered_map>

using namespace cellar;

namespace
{
  constexpr int WindowWidth = 1280;
  constexpr int WindowHeight = 800;
  constexpr const char* GlSlVersion = "#version 130";

  void glfwErrorCallback(int error, const char* description)
  {
    std::cerr << "GLFW error " << error << ": " << description << std::endl;
  }

  size_t romCount(const DatFile& dat)
  {
    size_t count = 0;
    for (const auto& game : dat.games)
      count += game.roms.size();
    return count;
  }

  std::string crcText(const DatRom& rom)
  {
    if (!rom.hash || !rom.hash->hash.crc32enabled)
      return "-";

    return fmt::format("{:08X}", rom.hash->hash.crc32);
  }

  std::string sizeText(const DatRom& rom)
  {
    if (!rom.hash || !rom.hash->hash.sizeEnabled)
      return "-";

    return strings::humanReadableSize(rom.hash->hash.size, true, 2);
  }

}

UserInterface::UserInterface(Kernel* kernel, const std::string& name) :
  KernelModule(kernel, name)
{
}

UserInterface::~UserInterface()
{
  destroyWindow();
}

bool UserInterface::createWindow()
{
  glfwSetErrorCallback(glfwErrorCallback);

  if (!glfwInit())
    return false;

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

  _window = glfwCreateWindow(WindowWidth, WindowHeight, "RetroZip Cellar", nullptr, nullptr);
  if (!_window)
  {
    glfwTerminate();
    return false;
  }

  glfwMakeContextCurrent(_window);
  glfwSwapInterval(1);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  configureStyle();

  ImGui_ImplGlfw_InitForOpenGL(_window, true);
  ImGui_ImplOpenGL3_Init(GlSlVersion);

  return true;
}

void UserInterface::destroyWindow()
{
  if (!_window)
    return;

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(_window);
  glfwTerminate();
  _window = nullptr;
}

void UserInterface::configureStyle()
{
  ImGui::StyleColorsDark();

  ImGuiStyle& style = ImGui::GetStyle();
  style.WindowRounding = 0.0f;
  style.ChildRounding = 4.0f;
  style.FrameRounding = 3.0f;
  style.GrabRounding = 3.0f;
  style.TabRounding = 3.0f;
  style.WindowBorderSize = 0.0f;
  style.FrameBorderSize = 0.0f;
  style.ItemSpacing = ImVec2(8.0f, 6.0f);
  style.WindowPadding = ImVec2(10.0f, 10.0f);
}

void UserInterface::init()
{
  if (!createWindow())
  {
    error("Unable to initialize GLFW/ImGui user interface");
    return;
  }

  selectRepository();

  while (!glfwWindowShouldClose(_window))
  {
    glfwPollEvents();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    drawFrame();

    ImGui::Render();

    int displayWidth = 0;
    int displayHeight = 0;
    glfwGetFramebufferSize(_window, &displayWidth, &displayHeight);
    glViewport(0, 0, displayWidth, displayHeight);
    glClearColor(0.07f, 0.08f, 0.10f, 1.00f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(_window);
  }

  destroyWindow();
}

void UserInterface::drawFrame()
{
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);

  constexpr ImGuiWindowFlags flags =
    ImGuiWindowFlags_NoDecoration |
    ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoSavedSettings |
    ImGuiWindowFlags_NoBringToFrontOnFocus;

  ImGui::Begin("Cellar", nullptr, flags);

  drawToolbar();
  ImGui::Separator();

  const float browserWidth = std::min(460.0f, ImGui::GetContentRegionAvail().x * 0.42f);
  const float consoleHeight = 220.0f;

  ImGui::BeginChild("RepositoryBrowser", ImVec2(browserWidth, 0.0f), true);
  drawRepositoryBrowser();
  ImGui::EndChild();

  ImGui::SameLine();

  ImGui::BeginGroup();
  ImGui::BeginChild("Inspector", ImVec2(0.0f, -consoleHeight - ImGui::GetStyle().ItemSpacing.y), true);
  drawInspector();
  ImGui::EndChild();

  ImGui::BeginChild("Console", ImVec2(0.0f, 0.0f), true);
  drawConsole();
  ImGui::EndChild();
  ImGui::EndGroup();

  ImGui::End();
}

void UserInterface::drawToolbar()
{
  const bool vfsRunning = kernel()->vfs()->isRunning();

  if (ImGui::Button(vfsRunning ? "Stop VFS" : "Start VFS"))
  {
    if (vfsRunning)
      kernel()->vfs()->stop();
    else
      kernel()->vfs()->start();
  }

  ImGui::SameLine();
  if (ImGui::Button("Repository"))
    selectRepository();

  ImGui::SameLine(ImGui::GetWindowWidth() - 260.0f);
  ImGui::TextDisabled("DATs: %zu | ROM hashes: %zu",
    kernel()->db()->dats().size(),
    kernel()->db()->hashesCount());
}

void UserInterface::drawRepositoryBrowser()
{
  if (ImGui::Selectable("Repository", _selectedTitle == "Repository"))
    selectRepository();

  ImGui::SeparatorText("Systems");

  std::unordered_map<const meta::Company*, std::vector<const meta::System*>> systemsByCompany;
  for (const auto& system : meta::Repository::i()->systems())
    systemsByCompany[system.company()].push_back(&system);

  std::vector<const meta::Company*> companies;
  companies.reserve(systemsByCompany.size());
  for (const auto& [company, systems] : systemsByCompany)
    companies.push_back(company);

  std::sort(companies.begin(), companies.end(), [](const auto& lhs, const auto& rhs) {
    return lhs->name() < rhs->name();
  });

  for (const auto* company : companies)
  {
    auto& systems = systemsByCompany[company];
    std::sort(systems.begin(), systems.end(), [](const auto& lhs, const auto& rhs) {
      return lhs->name() < rhs->name();
    });

    if (ImGui::TreeNodeEx(company->ident().c_str(), ImGuiTreeNodeFlags_DefaultOpen, "%s", company->name().c_str()))
    {
      for (const auto* system : systems)
        drawSystemNode(*system);

      ImGui::TreePop();
    }
  }
}

void UserInterface::drawSystemNode(const meta::System& system)
{
  size_t datCount = 0;
  for (const auto& [name, dat] : kernel()->db()->dats())
    if (dat.system == &system)
      ++datCount;

  ImGuiTreeNodeFlags flags = datCount == 0 ? ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen : 0;
  const bool open = ImGui::TreeNodeEx(system.ident().c_str(), flags, "%s (%zu)", system.longName().c_str(), datCount);

  if (ImGui::IsItemClicked())
    selectSystem(system);

  if (open && datCount > 0)
  {
    std::vector<const DatFile*> dats;
    for (const auto& [name, dat] : kernel()->db()->dats())
      if (dat.system == &system)
        dats.push_back(&dat);

    std::sort(dats.begin(), dats.end(), [](const auto* lhs, const auto* rhs) {
      return lhs->name < rhs->name;
    });

    for (const auto* dat : dats)
      drawDatNode(*dat);

    ImGui::TreePop();
  }
}

void UserInterface::drawDatNode(const DatFile& dat)
{
  ImGui::PushID(&dat);
  const bool open = ImGui::TreeNodeEx("dat", 0, "%s (%zu games)", dat.name.c_str(), dat.games.size());

  if (ImGui::IsItemClicked())
    selectDat(dat);

  if (open)
  {
    for (const auto& game : dat.games)
      drawGameNode(game);

    ImGui::TreePop();
  }

  ImGui::PopID();
}

void UserInterface::drawGameNode(const Game& game)
{
  ImGui::PushID(&game);

  if (game.hasSingleRom())
  {
    drawRomLeaf(game[0]);
    ImGui::PopID();
    return;
  }

  const bool open = ImGui::TreeNodeEx("game", 0, "%s (%zu roms)", game.name.c_str(), game.roms.size());

  if (ImGui::IsItemClicked())
    selectGame(game);

  if (open)
  {
    for (const auto& rom : game.roms)
      drawRomLeaf(rom);

    ImGui::TreePop();
  }

  ImGui::PopID();
}

void UserInterface::drawRomLeaf(const DatRom& rom)
{
  ImGui::PushID(&rom);
  ImGui::TreeNodeEx("rom", ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen, "%s", rom.name.c_str());

  if (ImGui::IsItemClicked())
    selectRom(rom);

  ImGui::PopID();
}

void UserInterface::drawInspector()
{
  ImGui::TextUnformatted(_selectedTitle.c_str());
  ImGui::Separator();
  ImGui::TextWrapped("%s", _selectedDetails.c_str());
}

void UserInterface::drawConsole()
{
  ImGui::TextUnformatted("Console");
  ImGui::SameLine(ImGui::GetWindowWidth() - 86.0f);
  if (ImGui::SmallButton("Clear"))
    _consoleMessages.clear();

  ImGui::Separator();

  ImGui::BeginChild("ConsoleScroll", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar);
  for (const auto& message : _consoleMessages)
    ImGui::TextUnformatted(message.c_str());

  if (_scrollConsoleToBottom)
  {
    ImGui::SetScrollHereY(1.0f);
    _scrollConsoleToBottom = false;
  }
  ImGui::EndChild();
}

void UserInterface::selectRepository()
{
  size_t games = 0;
  size_t roms = 0;

  for (const auto& [name, dat] : kernel()->db()->dats())
  {
    games += dat.games.size();
    roms += romCount(dat);
  }

  _selectedTitle = "Repository";
  _selectedDetails = fmt::format(
    "{} DAT files\n{} games\n{} ROM entries\n{} unique hash records",
    kernel()->db()->dats().size(),
    games,
    roms,
    kernel()->db()->hashesCount());
}

void UserInterface::selectSystem(const meta::System& system)
{
  size_t dats = 0;
  size_t games = 0;
  size_t roms = 0;

  for (const auto& [name, dat] : kernel()->db()->dats())
  {
    if (dat.system != &system)
      continue;

    ++dats;
    games += dat.games.size();
    roms += romCount(dat);
  }

  _selectedTitle = system.longName();
  _selectedDetails = fmt::format(
    "Short name: {}\nIdentifier: {}\nCompany: {}\nDAT files: {}\nGames: {}\nROM entries: {}",
    system.shortName(),
    system.ident(),
    system.company()->name(),
    dats,
    games,
    roms);
}

void UserInterface::selectDat(const DatFile& dat)
{
  _selectedTitle = dat.name;
  _selectedDetails = fmt::format(
    "Folder: {}\nSystem: {}\nGames: {}\nROM entries: {}",
    dat.folderName,
    dat.system ? dat.system->longName() : "Unknown",
    dat.games.size(),
    romCount(dat));
}

void UserInterface::selectGame(const Game& game)
{
  _selectedTitle = game.name;
  _selectedDetails = fmt::format("{} ROM entries", game.roms.size());
}

void UserInterface::selectRom(const DatRom& rom)
{
  _selectedTitle = rom.name;
  _selectedDetails = fmt::format("CRC32: {}\nSize: {}", crcText(rom), sizeText(rom));
}

void UserInterface::appendConsoleMessage(const std::string& message)
{
  _consoleMessages.push_back(message);
  _scrollConsoleToBottom = true;
}
