#include "thread_pool.h"

#include <pthread.h>
#include <iostream>


namespace misc {

ThreadPool::ThreadPool(const std::string &name, size_t num_threads)
{
    for (size_t i = 0; i < num_threads; ++i) {
        m_threads.emplace_back([this, name, i] {
            if (name.size()) {
                set_thread_name(name + std::to_string(i));
            }

            while (true) {
                function_t task;
                double weight;
                {
                    std::unique_lock lock(m_mutex);
                    m_queue_cv.wait(lock,
                        [this] { return m_stopped || m_tasks.size(); });
                    if (m_stopped && m_tasks.empty()) {
                        break;
                    }
                    std::tie(weight, task) = selectTask();
                    m_current_load += weight;
                }

                if (!task) {
                    continue;
                }

                m_queue_cv.notify_one();
                task();

                {
                    std::lock_guard lock(m_mutex);
                    m_current_load -= weight;
                    --m_unfinished_tasks;
                }
                m_wait_cv.notify_one();
            }
        });
    }
}


ThreadPool::ThreadPool(size_t num_threads)
    : ThreadPool("", num_threads)
{}


ThreadPool::~ThreadPool()
{
    stop();
}


ThreadPool::task_t ThreadPool::selectTask()
{
    // This is a very simple scheduler - just return any task that fits
    // the maximum possible load.
    //

    size_t running_tasks = m_unfinished_tasks - m_tasks.size();

    for (auto it = m_tasks.begin(); it != m_tasks.end(); ++it) {
        if (!running_tasks || m_current_load + it->first <= 1.) {
            auto task = std::move(*it);
            m_tasks.erase(it);
            return task;
        }
    }
    return task_t();
}


void ThreadPool::stop()
{
    {
        std::lock_guard lock(m_mutex);
        m_stopped = true;
    }
    m_queue_cv.notify_all();

    for (auto &thread : m_threads) {
        thread.join();
    }
    m_threads.clear();
}


void ThreadPool::wait()
{
    std::unique_lock lock(m_mutex);
    m_wait_cv.wait(lock, [this] { return m_unfinished_tasks == 0; });
}


void ThreadPool::set_thread_name(const std::string &name)
{
    if (pthread_setname_np(pthread_self(), name.c_str())) {
        std::cerr << "pthread_setname_np returned an error" << std::endl;
    }
}

}  // ~misc
