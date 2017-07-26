#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <future>
#include <functional>
#include <stdexcept>

class ThreadPool {
public:
    ThreadPool(size_t threads) : stop(false)
	{
		if(threads == 0)
		{
			throw std::invalid_argument("more than zero threads are expected");
		}

		workers.reserve(threads);
		for(;threads;--threads)
		{
			workers.emplace_back(
				[this]
				{
					while(true)
					{
						std::function<void()> task;
						{
							std::unique_lock<std::mutex> lock(this->queue_mutex);
							condition.wait(lock,
								[this]{ return this->stop || !this->tasks.empty(); });
							if(this->stop && this->tasks.empty())
								return;
							task = std::move(this->tasks.front());
							this->tasks.pop();
						}
						task();
					}
				}
			);
		}
	}

    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type>
	{
		using return_type = typename std::result_of<F(Args...)>::type;

		auto task = std::make_shared< std::packaged_task<return_type()> >(
				std::bind(std::forward<F>(f), std::forward<Args>(args)...)
				);

		auto res = task->get_future();
		{
			std::unique_lock<std::mutex> lock(queue_mutex);
			tasks.emplace([task](){ (*task)(); });
		}
		condition.notify_one();
		return res;

	}
    virtual ~ThreadPool()
	{
		stop = true;
		condition.notify_all();
		for(std::thread &worker: workers)
			worker.join();
	}
private:
    // need to keep track of threads so we can join them
    std::vector< std::thread > workers;
    // the task queue
    std::queue< std::function<void()> > tasks;
    
    // synchronization
    std::mutex queue_mutex;
    std::condition_variable condition;
    std::atomic_bool stop;
};

#endif//THREAD_POOL_H
