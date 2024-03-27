#pragma once

#include <vector>
#include <mutex>
#include <optional>
#include <functional>


namespace ppqsort::impl::cpp {

    template <typename taskType = std::function<void()>>
    class TaskStack {
        public:
            TaskStack(TaskStack&) = delete;

            TaskStack() {
                // reserve some space to prevent reallocations
                constexpr unsigned int reserved_tasks = 128;
                stack_.reserve(reserved_tasks);
            }

            bool try_push(taskType&& task) {
                const std::unique_lock lock(mutex_, std::try_to_lock);
                if (!lock.owns_lock()) return false;
                stack_.emplace_back(task);
                return true;
            }

            void push(taskType&& task) {
                std::lock_guard lock(mutex_);
                stack_.emplace_back(task);
            }

            std::optional<taskType> try_pop() {
                const std::unique_lock lock(mutex_, std::try_to_lock);
                if (!lock.owns_lock() || stack_.empty()) return std::nullopt;
                auto task = std::move(stack_.back());
                stack_.pop_back();
                return task;
            }

            std::optional<taskType> pop() {
                std::lock_guard lock(mutex_);
                if (stack_.empty()) return std::nullopt;
                auto task = std::move(stack_.back());
                stack_.pop_back();
                return task;
            }

        private:
            #ifdef GTEST_FLAG
            FRIEND_TEST(testThreadPool, PushBusyQueues);
            FRIEND_TEST(testThreadPool, PopBusyQueues);
            #endif

            std::vector<taskType> stack_;
            std::mutex mutex_;
    };
}