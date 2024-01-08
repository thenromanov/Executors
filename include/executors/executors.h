#pragma once

#include "executors/ubqueue.h"

#include <condition_variable>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

using Clock = std::chrono::system_clock;
using TimePoint = Clock::time_point;

enum class TaskState { Pending, Running, Completed, Failed, Canceled };

class Task;

using TaskSharedPtr = std::shared_ptr<Task>;

class Task : public std::enable_shared_from_this<Task> {
public:
    Task() = default;

    virtual void Run() = 0;

    void AddDependency(TaskSharedPtr dep) noexcept;

    void AddTrigger(TaskSharedPtr dep) noexcept;

    void SetTimeTrigger(TimePoint at) noexcept;

    bool IsPending() const noexcept;

    bool IsCompleted() const noexcept;

    bool IsFailed() const noexcept;

    bool IsCanceled() const noexcept;

    bool IsFinished() const noexcept;

    std::exception_ptr GetError() const noexcept;

    void TryExecute();

    void Cancel() noexcept;

    void Wait() noexcept;

    virtual ~Task() = default;

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;

    std::atomic<TaskState> state_ = TaskState::Pending;

    std::vector<TaskSharedPtr> dependencies_;
    std::vector<TaskSharedPtr> triggers_;
    TimePoint time_trigger_ = Clock::now();

    std::exception_ptr exception_;
};

template <typename T>
class Future final : public Task {
public:
    Future() = delete;

    Future(std::function<T()> fn) : fn_(std::move(fn)) {
    }

    void Run() final override {
        result_ = fn_();
    }

    T Get() {
        Wait();
        if (IsFailed()) {
            rethrow_exception(GetError());
        }
        return result_;
    }

    ~Future() final override = default;

private:
    std::function<T()> fn_;
    T result_;
};

template <typename T>
using FuturePtr = std::shared_ptr<Future<T>>;

// Used instead of void in generic code
struct Unit {};

class Executor {
public:
    Executor() = delete;

    Executor(size_t total_threads) {
        thread_pool_.reserve(total_threads);
        for (size_t i = 0; i < total_threads; ++i) {
            thread_pool_.emplace_back([this]() -> void {
                while (auto task = scheduler_.Pop()) {
                    if (!(*task) || (*task)->IsCanceled()) {
                        continue;
                    }
                    (*task)->TryExecute();
                    if (!(*task)->IsFinished()) {
                        scheduler_.Push(*task);
                    }
                }
            });
        }
    }

    void Submit(TaskSharedPtr task) noexcept {
        if (scheduler_.IsCanceled()) {
            task->Cancel();
            return;
        }
        if (task->IsPending()) {
            scheduler_.Push(std::move(task));
        }
    }

    void StartShutdown() noexcept {
        scheduler_.Cancel();
    }

    void WaitShutdown() noexcept {
        for (auto& cur_thread : thread_pool_) {
            if (cur_thread.joinable()) {
                cur_thread.join();
            }
        }
    }

    template <typename T>
    FuturePtr<T> Invoke(std::function<T()> fn) noexcept {
        auto task_ptr = std::make_shared<Future<T>>(fn);
        Submit(task_ptr);
        return task_ptr;
    }

    template <typename Y, typename T>
    FuturePtr<Y> Then(FuturePtr<T> input, std::function<Y()> fn) noexcept {
        auto task_ptr = std::make_shared<Future<Y>>(fn);
        task_ptr->AddDependency(input);
        Submit(task_ptr);
        return task_ptr;
    }

    template <typename T>
    FuturePtr<std::vector<T>> WhenAll(std::vector<FuturePtr<T>> all) noexcept {
        auto task_ptr = std::make_shared<Future<std::vector<T>>>([all]() -> std::vector<T> {
            std::vector<T> result;
            result.reserve(all.size());
            for (const auto& task : all) {
                result.push_back(task->Get());
            }
            return result;
        });
        for (const auto& dep : all) {
            task_ptr->AddDependency(dep);
        }
        Submit(task_ptr);
        return task_ptr;
    }

    template <typename T>
    FuturePtr<T> WhenFirst(std::vector<FuturePtr<T>> all) noexcept {
        auto task_ptr = std::make_shared<Future<T>>([all]() -> T {
            for (const auto& task : all) {
                if (task->IsFinished()) {
                    return task->Get();
                }
            }
            return all.front()->Get();
        });
        for (const auto& dep : all) {
            task_ptr->AddTrigger(dep);
        }
        Submit(task_ptr);
        return task_ptr;
    }

    template <typename T>
    FuturePtr<std::vector<T>> WhenAllBeforeDeadline(std::vector<FuturePtr<T>> all,
                                                    TimePoint deadline) noexcept {
        auto task_ptr = std::make_shared<Future<std::vector<T>>>([all]() -> std::vector<T> {
            std::vector<T> result;
            result.reserve(all.size());
            for (FuturePtr<T> task : all) {
                if (task->IsFinished()) {
                    result.push_back(task->Get());
                }
            }
            return result;
        });
        task_ptr->SetTimeTrigger(deadline);
        Submit(task_ptr);
        return task_ptr;
    }

    ~Executor() {
        StartShutdown();
        WaitShutdown();
    }

private:
    std::vector<std::jthread> thread_pool_;
    Queue<TaskSharedPtr> scheduler_;
};

std::shared_ptr<Executor> MakeThreadPoolExecutor(int num_threads);
