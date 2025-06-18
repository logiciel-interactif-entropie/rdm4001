#include "scheduler.hpp"

#include <unistd.h>

#include <chrono>

#include "input.hpp"
#include "logging.hpp"

#ifdef __linux
#include <linux/prctl.h> /* Definition of PR_* constants */
#include <pthread.h>
#include <sys/prctl.h>
#endif

#ifndef DISABLE_EASY_PROFILER
#include <easy/profiler.h>
#endif

#include "gfx/imgui/imgui.h"

static size_t schedulerId = 0;

namespace rdm {
Scheduler::Scheduler() { this->id = schedulerId++; }
Scheduler::~Scheduler() { waitToWrapUp(); }

void Scheduler::imguiDebug() {
  for (auto& job : jobs) {
    JobStatistics stats = job->getStats();
    ImGui::Text("Job %s", stats.name);
    ImGui::Text("S: %i, T: %0.2f", stats.schedulerId, stats.time);
    ImGui::Text("Total DT: %0.8f", stats.totalDeltaTime);
    ImGui::Text("DT: %0.8f", stats.deltaTime);
    ImGui::Text("Expected DT: %0.8f", job->getFrameRate());
    ImGui::Separator();
  }
}

void Scheduler::waitToWrapUp() {
  for (auto& job : jobs) {
    job->stopBlocking();
    if (job->getThread().joinable()) job->getThread().join();
  }
}

SchedulerJob* Scheduler::addJob(SchedulerJob* job) {
  jobs.push_back(std::unique_ptr<SchedulerJob>(job));
  job->getStats().schedulerId = id;
  return job;
}

void Scheduler::startAllJobs() {
  for (int i = 0; i < jobs.size(); i++) jobs[i]->startTask();
}

SchedulerJob::SchedulerJob(const char* name, bool stopOnCancel) {
  this->stopOnCancel = stopOnCancel;
  stats.name = name;
  killMutex.lock();
}

SchedulerJob::~SchedulerJob() { stopBlocking(); }

void JobStatistics::addDeltaTimeSample(double dt) {
  for (int i = 0; i < SCHEDULER_TIME_SAMPLES - 1; i++) {
    deltaTimeSamples[i] = deltaTimeSamples[i + 1];
  }
  // memcpy(deltaTimeSamples, &deltaTimeSamples[1],
  //      sizeof(double) * (SCHEDULER_TIME_SAMPLES - 1));
  deltaTimeSamples[SCHEDULER_TIME_SAMPLES - 1] = dt;
}

double JobStatistics::getAvgDeltaTime() {
  double avg = 0.0;
  for (int i = 0; i < SCHEDULER_TIME_SAMPLES; i++) avg += deltaTimeSamples[i];
  avg /= SCHEDULER_TIME_SAMPLES;
  return avg;
}

void SchedulerJob::task(SchedulerJob* job) {
#ifndef NDEBUG
#ifdef __linux
  std::string jobName = job->getStats().name;
  jobName += "/" + std::to_string(job->getStats().schedulerId);
  pthread_setname_np(pthread_self(), jobName.c_str());
  prctl(PR_SET_NAME, jobName.c_str());
#endif
#endif
  job->osPid = getpid();
  job->osTid = gettid();
#ifndef DISABLE_EASY_PROFILER
  EASY_THREAD_SCOPE(jobName.c_str());
#endif
  job->stats.time = 0.0;
  for (int i = 0; i < SCHEDULER_TIME_SAMPLES; i++)
    job->stats.deltaTimeSamples[i] = 0.0;
  bool running = true;
#ifndef NDEBUG
  Log::printf(LOG_DEBUG, "Starting job %s/%i", job->getStats().name,
              job->getStats().schedulerId);
#endif
  job->startup();
  while (running) {
#ifndef DISABLE_EASY_PROFILER
    EASY_BLOCK("Step");
#endif
    std::chrono::time_point start = std::chrono::steady_clock::now();

    Result r;
    try {
      r = job->step();
    } catch (std::exception& e) {
      Log::printf(LOG_FATAL,
                  "Fatal unhandled exception in %s/%i, what() = '%s'",
                  job->getStats().name, job->getStats().schedulerId, e.what());
      r = Cancel;

      try {
        job->error(e);
      } catch (std::exception& e) {
        Log::printf(LOG_FATAL,
                    "Double error in SchedulerJob error handler, what() = '%s'",
                    e.what());
      }

      Log::printf(LOG_DEBUG, "Sending quit object to Input queue");
      InputObject quitObject{.type = InputObject::Quit};
      Input::singleton()->postEvent(quitObject);
    }

    switch (r) {
      case Stepped:
        break;
      case Cancel:
        job->state = Stopped;
        break;
    }

    switch (job->state) {
      case Running:
      default:
        break;
      case StopPlease:
        if (job->stopOnCancel) {
          running = false;
          job->state = Stopped;
        }
        break;
      case Stopped:
        running = false;
        job->state = Stopped;
        break;
    }

    double frameRate = job->getFrameRate();
    std::chrono::time_point end = std::chrono::steady_clock::now();
    std::chrono::duration execution = end - start;
    if (frameRate != 0.0) {  // run as fast as we can if there is no frame rate
#ifndef DISABLE_EASY_PROFILER
      EASY_BLOCK("Sleep");
#endif
      std::chrono::duration sleep =
          std::chrono::duration<double>(frameRate) - execution -
          std::chrono::duration<double>(frameRate * 0.00599999999999);
      std::chrono::time_point until = end + sleep;
      if (job->stopOnCancel) {
        if (job->killMutex.try_lock_until(until)) {
          Log::printf(LOG_DEBUG, "killMutex unlocked on %s",
                      job->getStats().name);
          running = false;
        }
      } else {
        std::this_thread::sleep_until(until);
      }
    }
    job->stats.deltaTime = std::chrono::duration<double>(execution).count();
    end = std::chrono::steady_clock::now();
    execution = end - start;
    job->stats.totalDeltaTime =
        std::chrono::duration<double>(execution).count();
    job->stats.time += std::chrono::duration<double>(execution).count();
    job->stats.addDeltaTimeSample(job->stats.totalDeltaTime);
  }
  job->shutdown();
  job->state = Stopped;
#ifndef NDEBUG
  Log::printf(LOG_DEBUG, "Task %s/%i stopped", job->getStats().name,
              job->getStats().schedulerId);
#endif
}

void SchedulerJob::startTask() {
  thread = std::thread(&SchedulerJob::task, this);
}

SchedulerJob::Result SchedulerJob::step() { return Stepped; }

void SchedulerJob::stopBlocking() {
  if (state == Running) {
    state = StopPlease;
    interrupt();
    if (stopOnCancel) killMutex.unlock();
    while (state != Stopped) {
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    thread.join();
  }
}

SchedulerJob* Scheduler::getJob(std::string name) {
  for (int i = 0; i < jobs.size(); i++)
    if (jobs[i]->getStats().name == name) return jobs[i].get();
  return nullptr;
}
};  // namespace rdm
