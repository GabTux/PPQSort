#pragma once

#include <functional>
#include <mutex>
#include <queue>

namespace ppqsort::impl::cpp {
    template <typename taskType = std::function<void()>>
    class ThreadPool {
        public:
            ThreadPool(ThreadPool&) = delete;

            explicit ThreadPool(const unsigned int threads_count = std::thread::hardware_concurrency()) :
            threads_tasks_(threads_count) {
                for (unsigned int i = 0; i < threads_count; ++i) {
                    threads_priorities_.emplace_back(i);
                    threads_.emplace_back([this, i](const std::stop_token& st) { this->worker(st, i); });
                }
            }

            ~ThreadPool() {
                if (!stopped)
                    stop_and_wait();
            }

            void stop_and_wait() {
                for (size_t i = 0; i < threads_.size(); ++i) {
                    threads_[i].request_stop();
                    threads_tasks_[i].task_semaphore.release();
                    threads_[i].join();
                }
                stopped = true;
            }

            void push_task(taskType&& task) {
                // assign the task to the thread with the highest priority (it is at the front of the queue)
                // after that assign to the thread the lowest priority (move to the back of the queue)
                std::unique_lock priority_lock(mtx_priority_);
                const auto id = threads_priorities_.front();
                threads_priorities_.pop_front();
                threads_priorities_.push_back(id);
                priority_lock.unlock();

                task_count_.fetch_add(1, std::memory_order_release);
                threads_tasks_[id].tasks.push_back(std::move(task));
                threads_tasks_[id].task_semaphore.release();
            }

        private:
            void get_next_task(std::unique_lock<std::mutex>& my_queue_lock, const unsigned int id) {
                auto task = std::move(threads_tasks_[id].tasks.front());
                threads_tasks_[id].tasks.pop_front();
                my_queue_lock.unlock();
                task_count_.fetch_sub(1, std::memory_order_release);
                task();
            }

            void try_to_steal_task(const unsigned int id) {
                for (size_t i = 0; i < threads_tasks_.size(); ++i) {
                    if (i == id) continue;
                    std::unique_lock other_queue_lock(threads_tasks_[i].mutex);
                    if (!threads_tasks_[i].tasks.empty()) {
                        auto task = std::move(threads_tasks_[i].tasks.front());
                        threads_tasks_[i].tasks.pop_front();
                        other_queue_lock.unlock();
                        task_count_.fetch_sub(1, std::memory_order_release);
                        task();
                        return;
                    }
                }
            }

            void rotate_id_to_front(const unsigned int id) {
                std::unique_lock priority_lock(mtx_priority_);
                const auto iter = std::ranges::find(threads_priorities_, id);
                std::ignore = threads_priorities_.erase(iter);
                threads_priorities_.push_front(id);
                priority_lock.unlock();
            }

            void worker(const std::stop_token& stop_token, const unsigned int id) {
                while (true) {
                    // sleep until signalled about new tasks
                    threads_tasks_[id].task_semaphore.acquire();

                    // while there are tasks, execute them (mine or stolen)
                    while (task_count_.load(std::memory_order_acquire) > 0) {
                        std::unique_lock my_queue_lock(threads_tasks_[id].mutex);
                        if (!threads_tasks_[id].tasks.empty()) {
                            get_next_task(my_queue_lock, id);
                        } else {
                            my_queue_lock.unlock();
                            try_to_steal_task(id);
                        }
                    }

                    // no tasks left, check if we should finish
                    if (stop_token.stop_requested())
                        break;

                    // no tasks left, rotate me to front and sleep
                    rotate_id_to_front(id);
                }
            }

            struct TaskItem {
                std::mutex mutex;
                std::deque<taskType> tasks;
                std::binary_semaphore task_semaphore{0};
            };

            std::vector<std::jthread> threads_;
            std::deque<TaskItem> threads_tasks_;
            std::deque<unsigned int> threads_priorities_;
            alignas(parameters::cacheline_size) std::atomic<unsigned int> task_count_{0};
            std::mutex mtx_priority_;
            bool stopped = false;
    };
}