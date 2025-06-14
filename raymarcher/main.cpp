#include "launcher.hpp"
#include "raymarcher/rgame.hpp"
#include "settings.hpp"

int main(int argc, char** argv) {
  rdm::Settings::singleton()->parseCommandLine(argv, argc);
  rm::RGame game;
  game.mainLoop();
  rdm::Settings::singleton()->save();
}

LAUNCHER_DEFINE_EXPORTS(rm::RGame, "RayMarcher",
                        "RayMarcher (c) logiciel interactif entropie 2024");
