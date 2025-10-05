#include "raymarcher/rgame.hpp"
#include "settings.hpp"

int main(int argc, char** argv) {
  rdm::Settings::singleton()->parseCommandLine(argv, argc);
  rm::RGame game;
  game.mainLoop();
  rdm::Settings::singleton()->save();
}
