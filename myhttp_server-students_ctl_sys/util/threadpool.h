#pragma once
#include <functional>
#include <thread>
#include <queue>
#include <vector>
#include <condition_variable>
#include <mutex>

class ThreadPool 
{
public:
	ThreadPool (ThreadPool&) = delete;
	ThreadPool& operator= (ThreadPool&) = delete;
	ThreadPool(int thsize = 10):stop(false)
	{
		for (int i = 0; i < thsize; ++i)
		{
			threads.emplace_back([this] ()
				{
					while (1)
					{
						std::function<void()>task;//创建一个unique锁用来锁住进程
						{
							std::unique_lock<std::mutex> lock(mut);

							//wait是先判断表达式里的值是否为true，为true则不等了，为false则等待，只会进行开头这一次判断，
							//后面若想让他结束等待需要手动唤醒convar.notify_one(all)，
							//当一个线程执行到 convar.wait(lock) 时，它会阻塞自己并释放互斥锁，
							// 直到另一个线程调用 notify_one() 或 notify_all() 来唤醒它。即使任务队列中后来添加了新元素，
							// 等待的线程也不会自动醒来——它必须被显式地通知。

							convar.wait(lock, [this]()//加this是因为lamada表达式要使用类成员tasks和stop，需要捕获this才能调用类内成员
								{//若tasks为空则返回false，使继续等待，若不为空则返回true，处理任务
									return !tasks.empty() || stop;
								});
							//判断线程是否终止,而且要结束掉所有任务再终止
							if (stop && tasks.empty())
								return;
							//task来接收tasks里的函数，利用移动语义避免拷贝
							task = std::move(tasks.front());
							tasks.pop();//接收完后就可以pop tasks了
						}
						task();
					}
				}
			);
		}
	}
	~ThreadPool()
	{
		{
			std::unique_lock<std::mutex> Lock(mut);
			stop = true;
		}
		convar.notify_all();
		for (std::thread& t: threads)
		{
			t.join();
		}
	}
	template<class F,class... Args>
	void enqueue(F &&f,Args&&...args)//在函数声明里&&是万能引用，传入左值就是左值，右值就是右值，传左传右都可以
	{
		std::function<void()>task = 
			std::bind(std::forward<F>(f), std::forward<Args>(args)...);
		{
			std::unique_lock<std::mutex>lock(mut);
			tasks.emplace(task);
		}
		convar.notify_one();
	}
private:
	//线程数组
	std::vector<std::thread>threads;
	//任务队列
	std::queue<std::function<void()>>tasks;
    //互斥锁
	std::mutex mut;
	//条件变量
	std::condition_variable convar;
	//线程池是否终止
	bool stop;
};