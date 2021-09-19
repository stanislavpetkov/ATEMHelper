#pragma once
#include <cstdint>
#include <memory>
#include <deque>
#include <atomic>
#include "../Common/atomic_lock.h"


namespace pbn
{
	template<class _Ty,bool SelfCleaning = false, class _Alloc = std::allocator<_Ty>>
	class concurent_queue
	{
	private:
		size_t max_data_ = 0;
		std::atomic<bool> _data_lock = false;
		std::deque<_Ty, _Alloc> _dataq;
		void shrink_to_fit()
		{
			if constexpr (SelfCleaning == false)
			{
				return;
			}
			if (!_dataq.empty()) return;

			if (max_data_ > 128)
			{
				_dataq.shrink_to_fit();
				max_data_ = 0;
			}
			
		}
	public:
		bool try_pop(_Ty& data)
		{
			atomic_lock lock(_data_lock);
			if (_dataq.empty())
			{
				shrink_to_fit();								
				return false;
			}

			data = _dataq.front();
			_dataq.pop_front();
			return true;
		}
		void push(const _Ty& data)
		{
			atomic_lock lock(_data_lock);
			const auto sz = _dataq.size() + 1;
			if (max_data_ < sz)
			{
				max_data_ = sz;
			}
			return _dataq.push_back(data);
		}
		void push(_Ty&& data)
		{
			atomic_lock lock(_data_lock);
			const auto sz = _dataq.size() + 1;
			if (max_data_ < sz)
			{
				max_data_ = sz;
			}
			return _dataq.push_back(std::move(data));			
		}
		void clear()
		{
			atomic_lock lock(_data_lock);
			_dataq.clear();
			shrink_to_fit();
			
		}


		size_t size()
		{
			atomic_lock lock(_data_lock);
			return _dataq.size();
		}


		bool empty()
		{
			atomic_lock lock(_data_lock);
			return _dataq.empty();
		}
	};
}