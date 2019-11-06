#pragma once

#include <cassert>
#include <condition_variable>
#include <functional>
#include <future>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>


namespace misc {
namespace details {

template <typename T>
class Result;


class IResult
{
public:
    virtual ~IResult() {}

    // will raise stored exceptions if any
    //
    template <typename T>
    T get()
    {
        return dynamic_cast<Result<T>*>(this)->get();
    }

    // stored exceptions must not be raied here
    //
    virtual void wait() = 0;
    virtual void raise_stored_exceptions() = 0;
    virtual bool is_ready() const = 0;
};


template <typename T>
class Result : public IResult
{
public:
    Result() = default;
    Result(Result&&) = default;
    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;

    explicit Result(std::future<T> &&future)
        : m_future(std::move(future))
    {}

    ~Result() override {}


    T get()
    {
        if (!m_value.has_value()) {
            m_value = m_future.get();
        }
        return *m_value;
    }

    void wait() override
    {
        m_future.wait();
    }

    void raise_stored_exceptions() override
    {
        if (!m_value.has_value()) {
            get();
        }
    }

    bool is_ready() const override
    {
        const auto zero = std::chrono::seconds(0);
        return m_future.wait_for(zero) == std::future_status::ready;
    }

protected:
    std::future<T> m_future;
    std::optional<T> m_value;
    bool m_finished = false;
};


template <>
class Result<void> : public IResult
{
public:
    Result() = default;
    Result(Result&&) = default;
    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;

    explicit Result(std::future<void> &&future)
        : m_future(std::move(future))
    {}

    ~Result() override {}


    void get()
    {
        if (!m_finished) {
            m_future.get();
            m_finished = true;
        }
    }

    void wait() override
    {
        m_future.wait();
    }

    void raise_stored_exceptions() override
    {
        if (!m_finished) {
            get();
        }
    }

    bool is_ready() const override
    {
        const auto zero = std::chrono::seconds(0);
        return m_future.wait_for(zero) == std::future_status::ready;
    }

protected:
    std::future<void> m_future;
    bool m_finished = false;
};


template <>
inline void IResult::get()
{
    dynamic_cast<Result<void>*>(this)->get();
}

}  // ~details


class ThreadPool
{
public:
    ThreadPool(const std::string &name, size_t num_threads);
    ThreadPool(size_t num_threads);
    ~ThreadPool();

    ThreadPool(ThreadPool &&) = default;
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    typedef std::shared_ptr<details::IResult> ResultPtr;

    template <typename F, typename... Args>
    ResultPtr enqueue(double weight, F&& f, Args&&... args)
    {
        using return_type = decltype(f(args...));
        auto task_ptr = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));

        auto res = task_ptr->get_future();
        {
            std::unique_lock lock(m_mutex);
            assert(!m_stopped);
            m_tasks.push_back(task_t{ weight, [task_ptr] { (*task_ptr)(); } });
            ++m_unfinished_tasks;
        }
        m_queue_cv.notify_one();

        return std::make_shared<details::Result<return_type>>(std::move(res));
    }

    template <typename F, typename... Args>
    ResultPtr enqueue(F&& f, Args&&... args)
    {
        return enqueue(0., std::forward<F>(f), std::forward<Args>(args)...);
    }

    // block util all current tasks finished
    void wait();

    // Block until are enqueued task finished and stop threads.
    // A pool can not be used again after it was stopped.
    //
    void stop();

    size_t size() const { return m_threads.size(); }


private:
    std::vector<std::thread> m_threads;

    typedef std::function<void()> function_t;
    typedef std::pair<double, function_t> task_t;
    std::list<task_t> m_tasks;
    double m_current_load = 0.;

    std::mutex m_mutex;
    std::condition_variable m_queue_cv;
    bool m_stopped = false;

    std::condition_variable m_wait_cv;
    size_t m_unfinished_tasks = 0;

    task_t selectTask();
    void set_thread_name(const std::string &name);
};

}  // ~misc
