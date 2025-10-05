#pragma once
#include <functional>
#include <future>
#include <mutex>
#include <optional>
#include <source_location>
#include <thread>
#include <vector>

namespace rdm {
class WorkerManager {
  struct QueuedJob {
#ifndef NDEBUG
    std::source_location loc;
#endif
    std::function<void()> func;
  };

  struct Thread {
    std::mutex m;
    std::thread thread;
    std::promise<int>* promise;
    std::future<int> result;
    std::optional<QueuedJob> currentJob;
    int id;
  };

  bool running;

  void manager();

  std::thread managerThread;
  WorkerManager();

  std::mutex m;

  std::vector<Thread*> processingThreads;
  std::vector<Thread*> availableThreads;

  std::vector<QueuedJob> queuedJobs;

 public:
  static WorkerManager* singleton();

#ifndef NDEBUG
  void run(std::function<void()> f,
           std::source_location loc = std::source_location::current()) {
#else
  void run(std::function<void()> f) {
#endif
    std::scoped_lock l(m);
    QueuedJob job;
#ifndef NDEBUG
    job.loc = loc;
#endif
    job.func = f;
    queuedJobs.push_back(job);
  }

  void shutdown();
};
};  // namespace rdm
