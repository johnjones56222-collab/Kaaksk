// Fixed-size worker pool with a priority queue (lower value runs first).
//
// With zero threads the pool runs inline: tasks wait in the queue until the
// owner calls runPending() (used where threads are unavailable, e.g. a
// browser page that is not cross-origin isolated).
#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace strata {

class ThreadPool {
 public:
  explicit ThreadPool(unsigned threadCount);
  ~ThreadPool();
  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

  void submit(float priority, std::function<void()> task);
  // Blocks until the queue is empty and no task is running.
  void waitIdle();
  unsigned threadCount() const { return static_cast<unsigned>(workers_.size()); }
  bool inlineMode() const { return workers_.empty(); }
  size_t pending() const;
  // Inline mode: runs queued tasks on the calling thread, most urgent first,
  // until the queue is empty or `seconds` have passed (at least one task runs).
  // Returns the number of tasks run.
  size_t runPending(double seconds);

 private:
  struct Task {
    float priority;
    uint64_t order;  // FIFO among equal priorities
    std::function<void()> fn;
  };
  struct Compare {
    bool operator()(const Task& a, const Task& b) const {
      if (a.priority != b.priority) return a.priority > b.priority;
      return a.order > b.order;
    }
  };

  void workerLoop();
  static void runTask(std::function<void()>& fn);

  std::vector<std::thread> workers_;
  std::priority_queue<Task, std::vector<Task>, Compare> queue_;
  mutable std::mutex mutex_;
  std::condition_variable cv_;
  std::condition_variable idleCv_;
  uint64_t nextOrder_ = 0;
  unsigned running_ = 0;
  bool stopping_ = false;
};

}  // namespace strata
