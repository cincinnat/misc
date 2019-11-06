#include <gtest/gtest.h>
#include <cassert>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <chrono>
#include <cmath>
#include <map>
#include <any>

#include "src/thread_pool.h"


using namespace std::chrono_literals;


class Task
{
public:
    typedef std::function<void()> handler_t;

    Task(handler_t on_started = []{}, handler_t on_finished = []{})
        : m_on_started(on_started)
        , m_on_finished(on_finished)
    {}
    Task(Task &&other) = default;
    Task(const Task &) = delete;
    Task& operator=(const Task &other) = delete;

    ~Task()
    {
        stop();
        wait();
    }

    void run()
    {
        {
            std::lock_guard lock(sync_mutex);

            assert(!(m_finished || m_running));
            m_running = true;
            m_finished = false;
            m_on_started();
        }
        sync_cv.notify_all();

        {
            std::unique_lock lock(m_mutex);
            m_cv.wait(lock, [&]{ return (bool) m_stopped; });
        }

        {
            std::lock_guard lock(sync_mutex);

            m_finished = true;
            m_running = false;
            m_on_finished();
        }
        sync_cv.notify_all();
    }

    void stop()
    {
        {
            std::lock_guard lock(m_mutex);
            m_stopped = true;
        }
        m_cv.notify_all();
    };

    void wait()
    {
        std::unique_lock lock(sync_mutex);
        sync_cv.wait(lock, [&] { return (bool) m_finished; });
    }

    bool is_running()
    {
        return m_running;
    }

    bool finished()
    {
        return m_finished;
    }

    std::any& operator[](const std::string &key)
    {
        return m_data[key];
    }


    template< class Rep, class Period>
    static bool wait_for(const std::chrono::duration<Rep, Period>& duration,
        std::function<bool()> pred)
    {
        std::unique_lock lock(sync_mutex);
        return sync_cv.wait_for(lock, duration, pred);
    }

private:
    handler_t m_on_started;
    handler_t m_on_finished;

    static std::mutex sync_mutex;
    static std::condition_variable sync_cv;

    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::atomic_bool m_running = false;
    std::atomic_bool m_stopped = false;
    std::atomic_bool m_finished = false;

    std::map<std::string, std::any> m_data;
};

std::mutex Task::sync_mutex;
std::condition_variable Task::sync_cv;


TEST(ThreadPool, size)
{
    const size_t n = 5;
    misc::ThreadPool p("tests/size", n);

    size_t running = 0;

    std::list<Task> tasks;
    for (size_t i = 0; i < n * 2; ++i) {
        tasks.emplace_back([&]{ ++running; }, [&]{ --running; });
        p.enqueue(std::mem_fn(&Task::run), &tasks.back());
    }

    for (size_t i = 0; i < tasks.size(); ++i) {
        size_t tasks_left = tasks.size() - i;
        size_t expected_running = std::min(n, tasks_left);

        ASSERT_TRUE(Task::wait_for(1s, [&]{ return running == expected_running; }));

        for (auto& task : tasks) {
            if (task.is_running()) {
                task.stop();
                task.wait();
                break;
            }
        } }

    p.wait();
}


TEST(ThreadPool, weight)
{
    const size_t n = 5;
    misc::ThreadPool p("tests/size", n);

    size_t running = 0;
    double load = 0.;

    std::list<Task> tasks;
    for (size_t i = 0; i < n * 2; ++i) {
        double weight = (sin(i) + 1) * .6;
        auto& task = tasks.emplace_back(
            [&, weight]{ ++running; load += weight; },
            [&, weight]{ --running; load -= weight; });

        task["weight"] = weight;
        p.enqueue(weight, std::mem_fn(&Task::run), &task);
    }


    auto pool_is_full = [&] {
        for (auto &task : tasks) {
            // check if task is waiting in the thread pool's queue
            if (!(task.finished() || task.is_running())) {
                if (load + std::any_cast<double>(task["weight"]) <= 1.) {
                    return false;
                }
            }
        }
        // there are only queued tasks whose weight is > 1
        return running > 0;
    };

    for (size_t i = 0; i < tasks.size(); ++i) {
        ASSERT_TRUE(Task::wait_for(1s, pool_is_full));
        ASSERT_TRUE(running == 1 || load <= 1.);

        for (auto& task : tasks) {
            if (task.is_running()) {
                task.stop();
                task.wait();
                break;
            }
        }
    }

    p.wait();
}
