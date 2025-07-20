#pragma once
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace rdm {
class WorkerManager {
  struct Thread {
    std::mutex m;
    std::thread thread;
    bool doneExecuting;
  };

  bool running;

  void manager();

  std::thread managerThread;
  WorkerManager();

  std::mutex m;

  std::vector<Thread*> processingThreads;
  std::vector<Thread*> availableThreads;
  std::vector<std::function<void()>> queuedJobs;

 public:
  static WorkerManager* singleton();

  void run(std::function<void()> f) {
    std::scoped_lock l(m);
    queuedJobs.push_back(f);
  }

  void shutdown();
};
};  // namespace rdm
