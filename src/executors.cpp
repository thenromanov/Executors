#include "executors/executors.h"

void Task::AddDependency(TaskSharedPtr dep) noexcept {
    auto lock = std::scoped_lock{mutex_};
    dependencies_.push_back(dep);
}

void Task::AddTrigger(TaskSharedPtr dep) noexcept {
    auto lock = std::scoped_lock{mutex_};
    triggers_.push_back(dep);
}

void Task::SetTimeTrigger(TimePoint at) noexcept {
    auto lock = std::scoped_lock{mutex_};
    time_trigger_ = at;
}

bool Task::IsPending() const noexcept {
    return state_.load() == TaskState::Pending;
}

bool Task::IsCompleted() const noexcept {
    return state_.load() == TaskState::Completed;
}

bool Task::IsFailed() const noexcept {
    return state_.load() == TaskState::Failed;
}

bool Task::IsCanceled() const noexcept {
    return state_.load() == TaskState::Canceled;
}

bool Task::IsFinished() const noexcept {
    auto state = state_.load();
    return state != TaskState::Pending && state != TaskState::Running;
}

std::exception_ptr Task::GetError() const noexcept {
    auto lock = std::scoped_lock{mutex_};
    return exception_;
}

void Task::TryExecute() {
    auto lock = std::scoped_lock{mutex_};
    for (const auto& task : dependencies_) {
        if (!task->IsFinished()) {
            return;
        }
    }
    bool is_trigger_happened = triggers_.empty();
    for (const auto& task : triggers_) {
        is_trigger_happened |= task->IsFinished();
    }
    if (!is_trigger_happened) {
        return;
    }
    if (Clock::now() < time_trigger_) {
        return;
    }
    auto state = state_.load();
    if (state != TaskState::Pending) {
        return;
    }
    while (!state_.compare_exchange_weak(state, TaskState::Running)) {
        auto state = state_.load();
        if (state != TaskState::Pending) {
            return;
        }
    }
    try {
        this->Run();
    } catch (...) {
        state_ = TaskState::Failed;
        exception_ = std::current_exception();
        cv_.notify_all();
        return;
    }
    state_ = TaskState::Completed;
    cv_.notify_all();
}

void Task::Cancel() noexcept {
    auto state = state_.load();
    if (state != TaskState::Pending) {
        return;
    }
    while (!state_.compare_exchange_weak(state, TaskState::Canceled)) {
        auto state = state_.load();
        if (state != TaskState::Pending) {
            return;
        }
    }
    cv_.notify_all();
}

void Task::Wait() noexcept {
    auto lock = std::unique_lock{mutex_};
    cv_.wait(lock, [this]() -> bool { return IsFinished(); });
}

std::shared_ptr<Executor> MakeThreadPoolExecutor(int num_threads) {
    return std::make_shared<Executor>(num_threads);
}