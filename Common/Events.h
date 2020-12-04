#pragma once
#include <mutex>
#include <chrono>
#include <condition_variable>

class Event
{
private:
	std::mutex mtxEventWait;
	std::condition_variable cndSignalEvent;
	bool state, manual;
public:
	Event() :state(false), manual(false)
	{
	}

	Event(bool Initial, bool Manual) :state(Initial), manual(Manual)
	{
	}


	void change_manual(bool Manual)
	{
		std::lock_guard<std::mutex> lock(mtxEventWait); //std::lock_guard<std::mutex> kept for c++14 compatibility
		manual = Manual;
	}


	void reset()
	{
		std::lock_guard<std::mutex> lock(mtxEventWait); //std::lock_guard<std::mutex> kept for c++14 compatibility
		state = false;
	}

	bool wait(long milliseconds)
	{
		std::unique_lock<std::mutex> mtxWaitLock(mtxEventWait);
		
		if (manual && state) return true;

		auto nanos = std::chrono::high_resolution_clock::duration(std::chrono::milliseconds(milliseconds));
		if (!cndSignalEvent.wait_for(mtxWaitLock, nanos, [this] {
			return state;
			})) return false;

		if (!manual)
			state = false;

		return true;
	}


	void set()
	{
		std::lock_guard<std::mutex> lock(mtxEventWait); //std::lock_guard<std::mutex> kept for c++14 compatibility

		if (state)
			return;

		state = true;
		if (manual)
			cndSignalEvent.notify_all();
		else
			cndSignalEvent.notify_one();
	}


	void set_one()
	{
		std::lock_guard<std::mutex> lock(mtxEventWait); //std::lock_guard<std::mutex> kept for c++14 compatibility

		if (state) return;

		state = true;
		cndSignalEvent.notify_one();
	}

	void set_all()
	{
		std::lock_guard<std::mutex> lock(mtxEventWait); //std::lock_guard<std::mutex> kept for c++14 compatibility
		if (state) return;

		state = true;
		cndSignalEvent.notify_all();
	}
};



