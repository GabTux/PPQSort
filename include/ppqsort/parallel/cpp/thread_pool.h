#pragma once

#include <condition_variable>
#include <functional>
#include <queue>

namespace ppqsort::impl::cpp {
    template <typename taskType = std::function<void()>>
    class ThreadPool {
        public:
            ThreadPool(ThreadPool&) = delete;

            ~ThreadPool() {
                stop();
            }

            explicit ThreadPool(const size_t threads = std::thread::hardware_concurrency())
            : threads_count_(threads) {
                threads_.reserve(threads_count_);
                for (size_t i = 0; i < threads_count_; ++i) {
                    threads_.emplace_back([this] { this->worker(); });
                }
            }

            void push_task(taskType&& task) {
                {
                    std::lock_guard lck(task_mtx_);
                    task_queue_.emplace(std::forward<taskType>(task));
                }
                cv_task_.notify_one();
            }

            void stop() {
                {
                    std::lock_guard lck(task_mtx_);
                    stop_ = true;
                }
                cv_task_.notify_all();
                for (auto & it: threads_)
                    it.join();
            }

        private:
            std::queue<taskType> task_queue_;
            std::mutex task_mtx_;
            std::vector<std::thread> threads_;
            std::condition_variable cv_task_;
            const size_t threads_count_;
            bool stop_ = false;

            void worker() {
                while (true) {
                    std::unique_lock lck(task_mtx_);

                    while (task_queue_.empty()) {
                        if (stop_)
                            return;
                        cv_task_.wait(lck); //else
                    }

                    auto task = std::move(task_queue_.front());
                    task_queue_.pop();
                    lck.unlock();
                    task();
                }
            }
    };
}