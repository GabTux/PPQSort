
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


TEST(ThreadPool, OneThread) {
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

TEST(ThreadPool, StopEmpty) {
    using namespace ppqsort::impl::cpp;
    ThreadPool pool;
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

TEST(TaskStack, PushPop) {
    using namespace ppqsort::impl::cpp;
    TaskStack<int> stack;
    stack.push(1);
    stack.push(2);
    stack.push(3);
    ASSERT_EQ(stack.pop().value(), 3);
    ASSERT_EQ(stack.pop().value(), 2);
    ASSERT_EQ(stack.pop().value(), 1);
}

TEST(TaskStack, PopEmpty) {
    using namespace ppqsort::impl::cpp;
    TaskStack<int> stack;
    ASSERT_EQ(stack.pop().has_value(), false);
}

TEST(TaskStack, tryPushPop) {
    using namespace ppqsort::impl::cpp;
    TaskStack<int> stack;
    stack.try_push(1);
    stack.try_push(2);
    stack.try_push(3);
    ASSERT_EQ(stack.try_pop().value(), 3);
    ASSERT_EQ(stack.try_pop().value(), 2);
    ASSERT_EQ(stack.try_pop().value(), 1);
}