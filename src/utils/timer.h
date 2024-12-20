#ifndef __TIMER__H__
#define __TIMER__H__

#include <functional>
#include <thread>
#include <mutex>
#include <chrono>
#include <condition_variable>

namespace schwabcpp {

class Timer
{
public:
    Timer() : m_active(false) {}
    ~Timer() { stop(); }

    template<class Rep, class Period>
    void start(
        const std::chrono::duration<Rep, Period>& interval,
        std::function<void()> callback,
        bool fireOnStart = false)
    {
        // stop if it's already running
        std::unique_lock lock(m_mutex);
        if (m_active) {
            lock.unlock();
            stop();
        }

        m_active = true;
        m_timerThread = std::thread([this, interval, callback, fireOnStart] {
            std::unique_lock lock(m_mutex);
            while (m_active) {
                lock.unlock();

                if (fireOnStart) {
                    callback();
                }


                lock.lock();
                if (m_cv.wait_for(lock, interval, [this] { return !m_active; })) {
                    // exit if stop called
                    break;
                }

                if (!fireOnStart) {
                    callback();
                }
            }
        });
    }

    template<class Rep, class Period>
    void runOnce(
        const std::chrono::duration<Rep, Period>& delay,
        std::function<void()> callback)
    {
        // stop if it's already running
        std::unique_lock lock(m_mutex);
        if (m_active) {
            lock.unlock();
            stop();
        }

        m_active = true;
        m_timerThread = std::thread([this, delay, callback] {
            std::unique_lock lock(m_mutex);
            if (!m_cv.wait_for(lock, delay, [this] { return !m_active; })) {
                // fire on timeout only
                callback();
            }
        });
    }

    void stop()
    {
        {
            std::lock_guard lock(m_mutex);
            m_active = false;
        }
        m_cv.notify_all();
        if (m_timerThread.joinable()) {
            m_timerThread.join();
        }
    }

private:
    bool                      m_active;
    std::mutex                m_mutex;
    std::condition_variable   m_cv;
    std::thread               m_timerThread;
};

}

#endif // !__TIMER__H__
