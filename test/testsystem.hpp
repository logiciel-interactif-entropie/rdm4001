#pragma once
#include "game.hpp"
#include "testgame.hpp"

namespace test {
enum TestGroup {
  Base,
  Game,
  Network,
  Render,
};

class Test {
  std::string name;
  TestGroup group;

 public:
  Test(std::string name, TestGroup group);

  enum Result { Success, Check, Failed };

  std::string getName() { return name; }
  TestGroup getGroup() { return group; }

  virtual Result run(TestGame* game) = 0;
  virtual Result verify() { return Failed; };

  virtual void initializeGame(TestGame* game) {}
};

class System {
  System();
  std::map<TestGroup, std::vector<Test*>> tests;

 public:
  static System* singleton();

  void addTest(Test* test);
  int run();
};

template <typename T>
class TestInstantiator {
 public:
  TestInstantiator() { System::singleton()->addTest(new T()); }
};

#define TEST_ADD(x) static TestInstantiator<x> __##x = TestInstantiator<x>();
};  // namespace test
