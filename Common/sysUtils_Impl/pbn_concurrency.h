#pragma once
#include <mutex>
#include <algorithm>
#include <execution>
#include <ppltasks.h>

namespace pbn
{
	template<typename _ReturnType>
	class task_group
	{
	private:
		std::mutex mtx_;
		std::vector<concurrency::task<_ReturnType>> tasks_;
	public:
		void run(const std::function<_ReturnType()>& _Task)
		{
			std::unique_lock lock(mtx_);
			tasks_.emplace_back(concurrency::create_task(_Task));
		}

		Concurrency::task_status wait()
		{
			std::unique_lock lock(mtx_);
			return concurrency::when_all(tasks_.begin(), tasks_.end()).wait();
		}

		~task_group()
		{
			wait();
			tasks_.clear();
		}
	};



	inline void parallel_for(size_t start, size_t end, const std::function<void(size_t iter)>& _callable, size_t number_of_parallel_tasks)
	{



		if (end <= start) return;
		number_of_parallel_tasks = std::min(std::max(number_of_parallel_tasks, size_t(1)), static_cast<size_t>(std::thread::hardware_concurrency()));


		size_t number_of_items = (end - start);
		auto real_number_of_parallel_tasks = number_of_parallel_tasks > number_of_items ? number_of_items : number_of_parallel_tasks;

		if (real_number_of_parallel_tasks == 1)
		{
			for (size_t i = start; i < end; i++)
			{
				_callable(i);
			}
			return;
		}
		task_group<void> tg;



		size_t task_range = number_of_items / real_number_of_parallel_tasks;




		size_t local_start = start;
		//Divide range to tasks			

		//Execute
		for (size_t i = 0; i < real_number_of_parallel_tasks; i++)
		{
			if ((end - local_start) >= task_range)
			{
				tg.run([s = local_start, e = local_start + task_range, &_callable](){
					for (size_t j = s; j < e; j++)
					{
						_callable(j);
					}
				});
			}
			else
			{
				if (end - local_start <= 0) break;


				tg.run([s = local_start, e = end, &_callable]() {
					for (size_t i = s; i < e; i++)
					{
						_callable(i);
					}
					});
			}
			local_start += task_range;
		}



		//WaitForAll
		tg.wait();

	}


	template <typename _Iterator, typename _Function>
	inline void parallel_for_each(_Iterator _First, _Iterator _Last, const _Function& _Func, size_t number_of_parallel_tasks)
	{
		if (_Last == _First) return;
		number_of_parallel_tasks = std::min(std::max(number_of_parallel_tasks, size_t(1)), static_cast<size_t>(std::thread::hardware_concurrency()));

		size_t number_of_items = (_Last - _First);
		auto real_number_of_parallel_tasks = number_of_parallel_tasks > number_of_items ? number_of_items : number_of_parallel_tasks;


		if (real_number_of_parallel_tasks == 1)
		{
			std::for_each(_First, _Last, _Func);
			return;
		}


		task_group<void> tg;


		size_t task_range = number_of_items / real_number_of_parallel_tasks;




		auto local_start = _First;
		//Divide range to tasks			

		//Execute
		for (size_t i = 0; i < real_number_of_parallel_tasks; i++)
		{
			size_t rest = (_Last - local_start);
			if (rest >= task_range)
			{
				tg.run([s = local_start, e = local_start + task_range, &_Func](){
					std::for_each(s, e, _Func);
				});
			}
			else
			{
				if (rest > 0)
				{
					tg.run([s = local_start, e = _Last, &_Func]() {
						std::for_each(s, e, _Func);
						});
				}
				else
				{
					break;
				}
			}
			local_start += task_range;
		}



		//WaitForAll
		tg.wait();

	}

	template<typename Func1, typename Func2>
	inline void parallel_invoke(const Func1& f1, const Func2& f2)
	{
		task_group<void> tg;
		tg.run(f1);
		tg.run(f2);
		tg.wait();
	}

	template<typename Func1, typename Func2, typename Func3>
	inline void parallel_invoke(const Func1& f1, const Func2& f2, const Func3& f3)
	{
		task_group<void> tg;
		tg.run(f1);
		tg.run(f2);
		tg.run(f3);
		tg.wait();
	}

	template<typename Func1, typename Func2, typename Func3, typename Func4>
	inline void parallel_invoke(const Func1& f1, const Func2& f2, const Func3& f3, const Func4& f4)
	{
		task_group<void> tg;
		tg.run(f1);
		tg.run(f2);
		tg.run(f3);
		tg.run(f4);
		tg.wait();
	}
}