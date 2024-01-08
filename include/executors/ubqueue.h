#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>

template <typename T>
class Queue {
public:
    Queue() = default;

    bool Push(T value) noexcept {
        auto lock = std::scoped_lock{mutex_};
        if (is_canceled_) {
            return false;
        }
        data_.push(std::move(value));
        not_empty_.notify_one();
        return true;
    }

    std::optional<T> Pop() noexcept {
        auto lock = std::unique_lock{mutex_};
        not_empty_.wait(lock, [this] { return is_canceled_ || !data_.empty(); });
        if (is_canceled_ && data_.empty()) {
            return std::nullopt;
        }
        T result = std::move(data_.front());
        data_.pop();
        return result;
    }

    bool IsCanceled() const noexcept {
        auto lock = std::scoped_lock{mutex_};
        return is_canceled_;
    }

    void Cancel() noexcept {
        auto lock = std::scoped_lock{mutex_};
        is_canceled_ = true;
        not_empty_.notify_all();
    }

    ~Queue() {
        Cancel();
    }

private:
    std::queue<T> data_;

    mutable std::mutex mutex_;
    std::condition_variable not_empty_;

    bool is_canceled_ = false;
};