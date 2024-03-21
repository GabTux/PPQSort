
/****************************************************
 * ThreadPool Tests
 ****************************************************/

#include <gtest/gtest.h>
#include <future>
#include "ppqsort/parallel/cpp/thread_pool.h"

TEST(ThreadPool, TryPushConcurrent) {
    using namespace ppqsort::impl::cpp;
    ThreadPool pool(2);
    std::atomic<int> counter = 0;

    // Tasks to push values
    auto task = [&]() { pool.push_task([&]() {counter++;}); };

    // Push tasks concurrently in loop
    for (int i = 0; i < 1000; ++i) {
        std::ignore = std::async(std::launch::async, task);
    }

    pool.wait_and_stop();
    ASSERT_EQ(counter, 1000);
}


TEST(ThreadPool, OneThread) {
    using namespace ppqsort::impl::cpp;
    ThreadPool pool(1);
    std::atomic<int> counter = 0;

    // Tasks to push values
    auto task = [&]() { pool.push_task([&]() {counter++;}); };

    // Push tasks concurrently in loop
    for (int i = 0; i < 1000; ++i) {
        std::ignore = std::async(std::launch::async, task);
    }

    pool.wait_and_stop();
    ASSERT_EQ(counter, 1000);
}




TEST(ThreadPool, StopEmpty) {
    using namespace ppqsort::impl::cpp;
    ThreadPool pool;
}

TEST(ThreadPool, StopDuringPush) {
    using namespace ppqsort::impl::cpp;
    ThreadPool pool(2);
    std::atomic<bool> stop{false};
    std::atomic<int> counter{0};

    // Thread pushing tasks
    std::thread pusher([&] {
      while (!stop) {
        pool.push_task([&] { counter++; });
      }
    });

    // Thread stopping the pool
    std::thread stopper([&] {
      stop = true;
      pool.wait_and_stop();
    });

    pusher.join();
    stopper.join();
}

TEST(ThreadPool, PrematureExit) {
    using namespace ppqsort::impl::cpp;
    ThreadPool<> testPool(2);

    std::thread::id id_task_1, id_task_2, id_task_3, id_end;

    auto end = [&]() {
        id_end = std::this_thread::get_id();
    };

    auto task_2 = [&]() {
        id_task_2 = std::this_thread::get_id();
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