#include "settings.hpp"
#include "testsystem.hpp"

int main(int argc, char** argv) {
  rdm::Settings::singleton()->parseCommandLine(argv, argc);

  return test::System::singleton()->run();
}
