#include "input.hpp"
#include "testgame.hpp"
#include "testsystem.hpp"
#include "world.hpp"
namespace test {
class CreateWindowTest : public Test {
  Result result;

 public:
  CreateWindowTest() : Test("Initialize Client", Render) {}

  virtual Result run(TestGame* game) { return Check; }
  virtual Result verify() { return result; }
  virtual void initializeGame(TestGame* game) {
    try {
      game->startClient();
      result = Success;
    } catch (std::exception& e) {
      rdm::Log::printf(rdm::LOG_ERROR, "Couldn't initialize client, %s",
                       e.what());
      result = Failed;
    }
    game->getWorld()->setRunning(false);
  }
};

TEST_ADD(CreateWindowTest);
};  // namespace test
