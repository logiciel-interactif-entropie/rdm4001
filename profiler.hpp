#pragma once

#include <stdio.h>

#include <chrono>
#include <deque>
#include <map>
namespace rdm {
class SchedulerJob;

#define NR_BT_SAMPLES 64

class Profiler {
  bool firstFrame;
  SchedulerJob* job;
  std::chrono::time_point<std::chrono::steady_clock> frameStart;
  std::chrono::duration<float> frameTime;

  struct BlockTiming {
    std::deque<float> samples;

    float avg() {
      float v = 0.f;
      for (int i = 0; i < samples.size(); i++) v += samples[i];
      return v / samples.size();
    }
  };

  std::map<std::string, BlockTiming> timings;

 public:
  Profiler(SchedulerJob* job);

  struct Block {
    std::chrono::duration<float> begin;
    std::chrono::duration<float> time;
    std::chrono::duration<float> end;

    std::vector<Block> children;
    Block* parent;
    const char* name;

    Block() { parent = NULL; }
  };

  float getBlockAvg(std::string name) { return timings[name].avg(); }

  void frame();

  Block* getLastFrame() { return &oldFrame; }

  // finish with end
  void fun(const char* name);
  void end();

  void save();

 private:
  void saveBlock(FILE* file, Block* block, int lvl);
  std::deque<Block> savedBlocks;

  Block oldFrame;
  Block currentFrame;
  Block* currentBlock;
};
};  // namespace rdm
