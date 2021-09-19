#pragma once
#include <atomic>
#include <thread>

class atomic_lock
{
	std::atomic<bool>& locker;

	bool try_lock()
	{
		bool _false_ = false;
		if (!locker.compare_exchange_strong(_false_, true, std::memory_order::memory_order_acq_rel)) return false;
		return true;
	}

	bool unlock()
	{
		bool _true_ = true;
		if (!locker.compare_exchange_strong(_true_, false, std::memory_order::memory_order_acq_rel)) return false;
		return true;
	};

public:
	atomic_lock(std::atomic<bool>& locked) :locker(locked)
	{
		while (!try_lock())
		{
			std::this_thread::yield();
		}
	}

	~atomic_lock()
	{
		while (!unlock())
		{
			std::this_thread::yield();
		}
	}

};