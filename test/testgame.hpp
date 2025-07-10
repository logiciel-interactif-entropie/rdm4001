#pragma once
#include "game.hpp"
namespace test {
class Test;

class TestGame : public rdm::Game {
  Test* currentTest;

 public:
  TestGame();

  void setTest(Test* t) { currentTest = t; }

  virtual void initialize();
};
}  // namespace test
