#ifndef _OPENMP

/****************************************************
 * ThreadPool Tests
 ****************************************************/

#include <gtest/gtest.h>
#include <future>
#include "ppqsort/parallel/cpp/thread_pool.h"

namespace ppqsort::impl::cpp {
    TEST(testThreadPool, TryPushConcurrent) {
        using namespace ppqsort::impl::cpp;
        ThreadPool pool(2);
        std::atomic<int> counter = 0;

        auto task = [&]() { pool.push_task([&]() {++counter;}); };

        // Push tasks concurrently in loop and wait for them to finish
        std::vector<std::future<void>> futures;
        for (int i = 0; i < 1000; ++i) {
            futures.push_back(std::async(std::launch::async, task));
        }
        for (auto& f : futures) {
            f.wait();
        }

        pool.wait_and_stop();
        ASSERT_EQ(counter, 1000);
    }


    TEST(testThreadPool, OneThread) {
        using namespace ppqsort::impl::cpp;
        ThreadPool pool(1);
        std::atomic<int> counter = 0;

        auto task = [&]() { pool.push_task([&]() {counter++;}); };

        std::vector<std::future<void>> futures;
        for (int i = 0; i < 1000; ++i) {
            futures.push_back(std::async(std::launch::async, task));
        }
        for (auto& f : futures) {
            f.wait();
        }

        pool.wait_and_stop();
        ASSERT_EQ(counter, 1000);
    }

    TEST(testThreadPool, StopEmpty) {
        using namespace ppqsort::impl::cpp;
        ThreadPool pool;
    }

    TEST(testThreadPool, PrematureExit) {
        using namespace ppqsort::impl::cpp;
        ThreadPool<> testPool(2);

        std::thread::id id_task_1, id_end;

        auto end = [&]() {
            id_end = std::this_thread::get_id();
        };

        auto task_2 = [&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            testPool.push_task(end);
            std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        };

        auto task_1 = [&]() {
            id_task_1 = std::this_thread::get_id();
            testPool.push_task(task_2);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        };

        // task_1 pushes task_2 and sleeps, so both threads are busy and no tasks are in queue
        // task_1 finishes, no tasks in queue, but task_2 is still running --> task_1 must not exit
        // task_2 pushes end_task and sleeps, so the first thread should execute the end_task
        // in some other implementations, the first thread was stopped before task_2 finishes

        testPool.push_task(task_1);
        testPool.wait_and_stop();
        ASSERT_EQ(id_task_1, id_end);
    }


    TEST(testThreadPool, PushBusyQueues) {
        ThreadPool pool(2);
        std::atomic<int> counter{0};

        pool.threads_queues_[0].mutex_.lock();
        pool.threads_queues_[1].mutex_.lock();

        std::jthread t1([&](){
            // try to push, should fallback to waiting for the first queue
            pool.push_task([&](){ ++counter; });
        });

        // simulate busy queues for some time
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        pool.threads_queues_[0].mutex_.unlock();
        pool.threads_queues_[1].mutex_.unlock();
        pool.wait_and_stop();

        ASSERT_EQ(counter, 1);
    }

    TEST(testThreadPool, PopBusyQueues) {
        ThreadPool pool(2);
        std::atomic<int> counter{0};

        // push to all queues manually to avoid waking up the threads
        pool.threads_queues_[0].stack_.emplace_back([&](){ ++counter; });
        pool.threads_queues_[1].stack_.emplace_back([&](){ ++counter; });
        pool.total_tasks_ = 2;
        pool.pending_tasks_ = 2;

        // lock all queues
        pool.threads_queues_[0].mutex_.lock();
        pool.threads_queues_[1].mutex_.lock();


        std::jthread t1([&](){
            // push normally to wake up the threads, will wait until mutexes are unlocked
            pool.push_task([&](){ ++counter; });
        });

        // simulate busy queues for some time
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        pool.threads_queues_[0].mutex_.unlock();
        pool.threads_queues_[1].mutex_.unlock();

        pool.wait_and_stop();
        ASSERT_EQ(counter, 3);
    }


    /****************************************************
     * TaskStack Tests
    ****************************************************/

    TEST(testTaskStack, PushPop) {
        using namespace ppqsort::impl::cpp;
        TaskStack<int> stack;
        stack.push(1);
        stack.push(2);
        stack.push(3);
        ASSERT_EQ(stack.pop().value(), 3);
        ASSERT_EQ(stack.pop().value(), 2);
        ASSERT_EQ(stack.pop().value(), 1);
    }

    TEST(testTaskStack, PopEmpty) {
        using namespace ppqsort::impl::cpp;
        TaskStack<int> stack;
        ASSERT_EQ(stack.pop().has_value(), false);
    }

    TEST(testTaskStack, tryPushPop) {
        using namespace ppqsort::impl::cpp;
        TaskStack<int> stack;
        stack.try_push(1);
        stack.try_push(2);
        stack.try_push(3);
        ASSERT_EQ(stack.try_pop().value(), 3);
        ASSERT_EQ(stack.try_pop().value(), 2);
        ASSERT_EQ(stack.try_pop().value(), 1);
    }
}
#endif