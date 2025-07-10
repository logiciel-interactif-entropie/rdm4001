#include "testsystem.hpp"

#include <thread>

#include "game.hpp"
#include "settings.hpp"
#include "testgame.hpp"

namespace test {
static System* _singleton = NULL;
System* System::singleton() {
  if (!_singleton) _singleton = new System();
  return _singleton;
}

System::System() {}

void System::addTest(Test* test) { tests[test->getGroup()].push_back(test); }

static rdm::CVar testSelectedGroup("group", "", CVARF_CONSOLE_ARGUMENT);

int System::run() {
  std::map<std::string, TestGroup> translate = {
      {"Base", Base}, {"Game", Game}, {"Network", Network}, {"Render", Render}};
  if (translate.find(testSelectedGroup.getValue()) == translate.end()) {
    rdm::Log::printf(rdm::LOG_FATAL, "Unknown group %s",
                     testSelectedGroup.getValue().c_str());
    return -1;
  }

  int passed = 0;
  int failed = 0;
  int tested = 0;

  for (auto currentTest : tests[translate[testSelectedGroup.getValue()]]) {
    rdm::Log::printf(rdm::LOG_INFO, "Starting test '%s'",
                     currentTest->getName().c_str());
    tested++;

    bool xit = false;

    {
      TestGame game;
      game.setTest(currentTest);
      switch (currentTest->run(&game)) {
        case Test::Success:
          rdm::Log::printf(rdm::LOG_INFO, "Test '%s' passed",
                           currentTest->getName().c_str());
          passed++;
          xit = true;
          break;
        case Test::Failed:
          rdm::Log::printf(rdm::LOG_FATAL, "Test '%s' failed",
                           currentTest->getName().c_str());
          failed++;
          xit = true;
          break;
        case Test::Check:
          rdm::Log::printf(rdm::LOG_DEBUG, "Test '%s' is check, starting loop",
                           currentTest->getName().c_str());
          break;
      }

      if (xit) continue;

      game.mainLoop();
    }

    switch (currentTest->verify()) {
      case Test::Success:
        rdm::Log::printf(rdm::LOG_INFO, "Test '%s' passed",
                         currentTest->getName().c_str());
        passed++;
        break;
      case Test::Check:
        rdm::Log::printf(rdm::LOG_DEBUG, "??? verify returned Check");
      case Test::Failed:
        rdm::Log::printf(rdm::LOG_FATAL, "Test '%s' failed",
                         currentTest->getName().c_str());
        failed++;
        break;
    }
  }

  rdm::Log::printf(rdm::LOG_INFO, "%i tests, %i passed, %i failed", tested,
                   passed, failed);

  if (failed) return -1;
  return 0;
}

Test::Test(std::string name, TestGroup group) {
  this->name = name;
  this->group = group;
}

class VerifyTest : public Test {
 public:
  VerifyTest() : Test("Verify Sanity", Base) {}

  virtual Result run(TestGame* game) { return Success; }
};

TEST_ADD(VerifyTest)
};  // namespace test
