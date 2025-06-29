#include "profiler.hpp"

#include <chrono>
#include <cstdio>
#include <format>

#include "console.hpp"
#include "game.hpp"
#include "settings.hpp"

namespace rdm {
#ifdef NDEBUG
static CVar profiler_enable("profiler_enable", "0", CVARF_SAVE | CVARF_GLOBAL);
#else
static CVar profiler_enable("profiler_enable", "1", CVARF_SAVE | CVARF_GLOBAL);
#endif

static CVar profiler_save_blocks("profiler_save_blocks", "0", CVARF_GLOBAL);
static ConsoleCommand profiler_save(
    "profiler_save", "profiler_save [client/server] [job name]",
    "saves profiler blocks to file", [](Game* game, ConsoleArgReader reader) {
      if (!profiler_save_blocks.getBool())
        throw std::runtime_error("Enable profiler_save_blocks to save blocks");

      std::string target = reader.next();
      std::string job = reader.next();

      Profiler* profiler;
      if (target == "client")
        profiler =
            &game->getWorld()->getScheduler()->getJob(job)->getProfiler();
      else if (target == "server")
        profiler =
            &game->getServerWorld()->getScheduler()->getJob(job)->getProfiler();
      else
        throw std::runtime_error("argument #1 must be client or server");

      profiler->save();
    });

Profiler::Profiler(SchedulerJob* job) {
  firstFrame = true;
  frameStart = std::chrono::steady_clock::now();
  oldFrame.begin = std::chrono::steady_clock::now() - frameStart;
  oldFrame.end = std::chrono::steady_clock::now() - frameStart;
  oldFrame.time = oldFrame.end - oldFrame.begin;
  oldFrame.name = "???";
  this->job = job;
}

void Profiler::saveBlock(FILE* file, Block* block, int lvl) {
  std::string tabs("");
  for (int i = 0; i < lvl; i++) tabs.push_back('\t');
  fprintf(file, "%sblock %s %0.16f %0.16f %0.16f\n", tabs.c_str(), block->name,
          block->begin.count(), block->end.count(), block->time.count());
  for (auto block : block->children) {
    saveBlock(file, &block, lvl + 1);
  }
  fprintf(file, "%send\n", tabs.c_str());
}

void Profiler::save() {
  FILE* out = fopen(
      std::format("profiler_{}.blocks", job->getStats().name).c_str(), "w");
  if (out) {
    fputs("If U are Reading This. U are Not Welcome\n", out);
    int frame = 0;
    for (auto block : savedBlocks) {
      fprintf(out, "frame %i\n", frame);
      saveBlock(out, &block, 0);
      frame++;
    }
  } else {
    throw std::runtime_error("fopen == NULL");
  }
}

void Profiler::frame() {
  if (!profiler_enable.getBool()) return;

  if (firstFrame) {
    firstFrame = false;
    frameTime = std::chrono::duration<float>(0.f);
  } else {
    frameTime = std::chrono::steady_clock::now() - frameStart;
    currentFrame.end = frameTime;
    currentFrame.time = currentFrame.end - currentFrame.begin;
    if (profiler_save_blocks.getBool()) savedBlocks.push_back(oldFrame);
    oldFrame = currentFrame;
  }
  frameStart = std::chrono::steady_clock::now();
  {
    currentFrame = Block();
  }
  currentFrame.begin = std::chrono::steady_clock::now() - frameStart;
  currentFrame.name = "Frame";
  currentBlock = &currentFrame;
}

void Profiler::fun(const char* name) {
  if (!profiler_enable.getBool()) return;

  Block b;
  b.begin = std::chrono::steady_clock::now() - frameStart;
  b.parent = currentBlock;
  b.name = name;
  currentBlock->children.push_back(b);
  currentBlock = &currentBlock->children.back();
}

void Profiler::end() {
  if (!profiler_enable.getBool()) return;

  currentBlock->end = std::chrono::steady_clock::now() - frameStart;
  currentBlock->time = currentBlock->end - currentBlock->begin;

  timings[currentBlock->name].samples.push_back(currentBlock->time.count());
  if (timings[currentBlock->name].samples.size() == NR_BT_SAMPLES)
    timings[currentBlock->name].samples.pop_front();

  currentBlock = currentBlock->parent;
}
};  // namespace rdm
