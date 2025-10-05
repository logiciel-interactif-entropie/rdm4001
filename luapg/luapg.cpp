#include "luapg.hpp"

#include "json.hpp"
#include "logging.hpp"
#include "pak_file.hpp"
#include "script/script.hpp"
#include "script/script_context.hpp"
#include "settings.hpp"

using json = nlohmann::json;
namespace luapg {
RDM_REFLECTION_BEGIN_DESCRIBED(LuaPGGame)
RDM_REFLECTION_PROPERTY_EVENT(LuaPGGame, ServerStarted,
                              &LuaPGGame::getServerStarting);
RDM_REFLECTION_END_DESCRIBED()

void LuaPGGame::initialize() {
  getWorldConstructorSettings().network = true;
  startClient();
}

void LuaPGGame::initializeClient() {
  rdm::script::ScriptContext* scriptContext = getWorld()->getScriptContext();
  lua_State* l = scriptContext->getLuaState();

  auto mp = common::FileSystem::singleton()->readFile("luapg/modlist.json");
  std::string ms = std::string(mp.value().begin(), mp.value().end());
  json p = json::parse(ms);

  int ord = 0;
  for (auto mod : p["Mods"]) {
    rdm::Log::printf(rdm::LOG_INFO, "Loading mod %s",
                     std::string(mod["Path"]).c_str());
    std::string xec = mod["Exec"];
    pak::PakFile* pakF = new pak::PakFile(std::string(mod["Path"]).c_str());
    common::FileSystem::singleton()->addApi(pakF, mod["URI"], ord++);

    rdm::script::Script* script = scriptContext->newScript(xec.c_str());
    if (script) {
      script->run();
    }
  }
}

void LuaPGGame::initializeServer() { serverStarting.fire(); }
};  // namespace luapg

int main(int argc, char** argv) {
  rdm::Settings::singleton()->parseCommandLine(argv, argc);
  luapg::LuaPGGame game;
  game.mainLoop();
  rdm::Settings::singleton()->save();
}
