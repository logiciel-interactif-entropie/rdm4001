#include "worker.hpp"

#include <thread>

#include "fun.hpp"
#include "logging.hpp"
namespace rdm {
WorkerManager::WorkerManager() {
  Log::printf(LOG_DEBUG, "Starting worker manager with %i worker(s)",
              Fun::getNumCpus());

  running = true;
  managerThread = std::thread(std::bind(&WorkerManager::manager, this));
  for (int i = 0; i < Fun::getNumCpus(); i++) {
    availableThreads.push_back(new Thread());
  }
}

void WorkerManager::manager() {
  while (running) {
    {
      std::scoped_lock l(m);
      while (queuedJobs.size()) {
        auto job = queuedJobs.back();
        if (availableThreads.size()) {
          Thread* th = availableThreads.back();
          availableThreads.pop_back();
          th->doneExecuting = false;
          th->thread = std::thread([th, job] {
            job();
            th->doneExecuting = true;
          });
          processingThreads.push_back(th);
        } else
          break;
        queuedJobs.pop_back();
      }
      if (processingThreads.size()) {
        Thread* th = processingThreads.back();
        if (th->doneExecuting) {
          if (th->thread.joinable()) th->thread.join();
          processingThreads.pop_back();
          th->doneExecuting = false;
          availableThreads.push_back(th);
        }
      }
    }
    std::this_thread::yield();
  }

  if (processingThreads.size()) {
    for (auto& th : processingThreads) {
      if (th->doneExecuting) {
        delete th;
      } else {
        Log::printf(LOG_WARN, "Thread not done executing, waiting...");
        th->thread.join();
        delete th;
      }
    }
  }

  for (auto& th : availableThreads) {
    delete th;
  }

  Log::printf(LOG_DEBUG, "Worker manager stopped");
}

static WorkerManager* _singleton = NULL;
WorkerManager* WorkerManager::singleton() {
  if (!_singleton) _singleton = new WorkerManager();
  return _singleton;
}

void WorkerManager::shutdown() {
  running = false;
  managerThread.join();
}
};  // namespace rdm
