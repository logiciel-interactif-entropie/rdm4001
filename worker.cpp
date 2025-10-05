#include "worker.hpp"

#include <chrono>
#include <exception>
#include <source_location>
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
    Thread* th = new Thread();
    th->id = i;
    availableThreads.push_back(th);
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
          th->currentJob = job;
          availableThreads.pop_back();
          th->promise = new std::promise<int>();
          th->thread = std::thread([th, job] {
            try {
              job.func();
              th->promise->set_value_at_thread_exit(0);
            } catch (std::exception& e) {
              th->promise->set_exception_at_thread_exit(
                  std::current_exception());
            }
          });
          th->result = th->promise->get_future();
          processingThreads.push_back(th);
        } else
          break;
        queuedJobs.pop_back();
      }
      if (processingThreads.size()) {
        Thread* th = processingThreads.back();
        if (th->result.valid()) {
          try {
            int result = th->result.get();
            if (result != 0)
              Log::printf(LOG_DEBUG, "Worker job returned %i", result);
          } catch (std::exception& e) {
            Log::printf(LOG_FATAL, "Worker job unhandled error: %s, exiting",
                        e.what());
#ifndef NDEBUG
            std::source_location& loc = th->currentJob.value().loc;
            Log::printf(LOG_ERROR, "Worker job location: %s in %s:%i",
                        loc.function_name(), loc.file_name(), loc.line());
#endif
            exit(-1);
          }

          if (th->thread.joinable()) th->thread.join();
          processingThreads.pop_back();
          th->currentJob = {};
          delete th->promise;
          availableThreads.push_back(th);
        }
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }

  if (processingThreads.size()) {
    for (auto& th : processingThreads) {
      if (th->result.valid()) {
        delete th;
      } else {
        Log::printf(LOG_WARN, "Thread not done executing, waiting...");
        th->thread.join();
        delete th;
      }
    }

    processingThreads.clear();
  }

  for (auto& th : availableThreads) {
    if (th->thread.joinable()) {
      th->thread.join();
    }
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
