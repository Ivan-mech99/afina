#ifndef AFINA_CONCURRENCY_EXECUTOR_H
#define AFINA_CONCURRENCY_EXECUTOR_H

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <algorithm>

namespace Afina {
namespace Concurrency {

/**
 * # Thread pool
 */
class Executor {
    enum class State {
        // Threadpool is fully operational, tasks could be added and get executed
        kRun,

        // Threadpool is on the way to be shutdown, no ned task could be added, but existing will be
        // completed as requested
        kStopping,

        // Threadppol is stopped
        kStopped
    };
   public:
    void Start()
    {
     std::unique_lock<std::mutex> lock(mutex);
     for (int i = 0; i < low_watermark; i++) {
        threads.emplace_back(std::thread([this] { return perform(this); }));
     }
     state = State::kRun;
    }

    Executor(int size, size_t min_num = 2, size_t max_num = 6, size_t wait_tm = 3000): max_queue_size(size),
                                                                                       low_watermark(min_num),
                                                                                       high_watermark(max_num),
                                                                                       idle_time(wait_tm) {}
    ~Executor() {Stop(true);}

    /**
     * Signal thread pool to stop, it will stop accepting new jobs and close threads just after each become
     * free. All enqueued jobs will be complete.
     *
     * In case if await flag is true, call won't return until all background jobs are done and all threads are stopped
     */
    void Stop(bool await = false){
        std::unique_lock<std::mutex> lock(mutex);
        state = State::kStopping;
        empty_condition.notify_all();
        for (auto &thread : threads) {
            if (await) {
                thread.join();
            }
        }
        if (threads.empty())
        {
        state = State::kStopped;
        }  
      }

    /**
     * Add function to be executed on the threadpool. Method returns true in case if task has been placed
     * onto execution queue, i.e scheduled for execution and false otherwise.
     *
     * That function doesn't wait for function result. Function could always be written in a way to notify caller about
     * execution finished by itself
     */
    template <typename F, typename... Types> bool Execute(F &&func, Types... args) {
        // Prepare "task"
        auto exec = std::bind(std::forward<F>(func), std::forward<Types>(args)...);
        std::unique_lock<std::mutex> lock(mutex);
        if (state!=State::kRun||(tasks.size()>=max_queue_size&&threads.size()==high_watermark))
           {return false;}
        else if(threads.size()<high_watermark&&threads.size()==worked_threads)
        {threads.emplace_back(std::thread([this] {return perform(this);}));}
        lock.unlock();
        // Enqueue new task
        tasks.push_back(exec);
        empty_condition.notify_one();
        return true;
    }

private:
    // No copy/move/assign allowed
    Executor(const Executor &);            // = delete;
    Executor(Executor &&);                 // = delete;
    Executor &operator=(const Executor &); // = delete;
    Executor &operator=(Executor &&);      // = delete;

    /**
     * Main function that all pool threads are running. It polls internal task queue and execute tasks
     */
    friend void perform(Executor *executor) 
    {
        std::unique_lock<std::mutex> lock(executor->mutex);
        while (executor->state == Executor::State::kRun){
            if (executor->tasks.empty()){
                auto start = std::chrono::steady_clock::now();
                auto result = executor->empty_condition.wait_until(lock, start + std::chrono::milliseconds(executor->idle_time));
                if (result == std::cv_status::timeout && executor->threads.size() > executor->low_watermark){
                    break;
                } else {
                    continue;
                }
            }
        auto task = std::move(executor->tasks.front());
        executor->tasks.pop_front();
        executor->worked_threads++;
        lock.unlock();
        task();
        lock.lock();
        executor->worked_threads--;
    }
    auto this_thread = std::this_thread::get_id();
    auto it = std::find_if(executor->threads.begin(), executor->threads.end(), [this_thread](std::thread &x){return x.get_id() == this_thread; });
    executor->threads.erase(it);
    if (executor->threads.empty()){
        executor->empty_condition.notify_all();
    }
   }
    /**
     * Mutex to protect state below from concurrent modification
     */
    std::mutex mutex;

    /**
     * Conditional variable to await new data in case of empty queue
     */
    std::condition_variable empty_condition;

    /**
     * Vector of actual threads that perorm execution
     */
    std::vector<std::thread> threads;

    /**
     * Task queue
     */
    std::deque<std::function<void()>> tasks;

    /**
     * Flag to stop bg threads
     */
    State state = State::kRun;
    size_t low_watermark;
    size_t high_watermark;
    size_t max_queue_size;
    size_t idle_time;
    size_t worked_threads = 0;
};

} // namespace Concurrency
} // namespace Afina

#endif // AFINA_CONCURRENCY_EXECUTOR_H
