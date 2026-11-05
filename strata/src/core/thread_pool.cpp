#include "core/thread_pool.hpp"

#include <chrono>
#include <exception>

#include "core/log.hpp"

namespace strata {

ThreadPool::ThreadPool(unsigned threadCount) {
  workers_.reserve(threadCount);
  for (unsigned i = 0; i < threadCount; ++i) workers_.emplace_back([this] { workerLoop(); });
}

ThreadPool::~ThreadPool() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stopping_ = true;
    // Drop work that has not started; running tasks finish normally.
    while (!queue_.empty()) queue_.pop();
  }
  cv_.notify_all();
  for (std::thread& t : workers_) t.join();
}

void ThreadPool::submit(float priority, std::function<void()> task) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopping_) return;
    queue_.push(Task{priority, nextOrder_++, std::move(task)});
  }
  cv_.notify_one();
}

void ThreadPool::waitIdle() {
  if (inlineMode()) {
    while (runPending(1e9) > 0) {
    }
    return;
  }
  std::unique_lock<std::mutex> lock(mutex_);
  idleCv_.wait(lock, [this] { return queue_.empty() && running_ == 0; });
}

size_t ThreadPool::runPending(double seconds) {
  using Clock = std::chrono::steady_clock;
  const Clock::time_point end = Clock::now() + std::chrono::duration_cast<Clock::duration>(
                                                    std::chrono::duration<double>(seconds));
  size_t ran = 0;
  do {
    std::function<void()> fn;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (queue_.empty()) break;
      fn = std::move(const_cast<Task&>(queue_.top()).fn);
      queue_.pop();
    }
    runTask(fn);  // may submit more work
    ++ran;
  } while (Clock::now() < end);
  return ran;
}

void ThreadPool::runTask(std::function<void()>& fn) {
  try {
    fn();
  } catch (const std::exception& e) {
    LOG_ERROR("worker task threw: %s", e.what());
  } catch (...) {
    LOG_ERROR("worker task threw an unknown exception");
  }
}

size_t ThreadPool::pending() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return queue_.size() + running_;
}

void ThreadPool::workerLoop() {
  for (;;) {
    std::function<void()> fn;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
      if (stopping_ && queue_.empty()) return;
      // priority_queue::top is const; the task is moved out before pop.
      fn = std::move(const_cast<Task&>(queue_.top()).fn);
      queue_.pop();
      ++running_;
    }
    runTask(fn);
    {
      std::lock_guard<std::mutex> lock(mutex_);
      --running_;
      if (queue_.empty() && running_ == 0) idleCv_.notify_all();
    }
  }
}

}  // namespace strata
