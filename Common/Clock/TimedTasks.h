#pragma once
#include "Clock.h"
#include <functional>
#include <memory>


enum class wait_t
{
	event,
	timeout,
	aborted,
	unknown
};

struct TimeStore
{
	int64_t clock_time_base;
	int64_t start_time;
	int64_t actual_time;
	int64_t event_time_start;
	int64_t event_time_end;

	int64_t sample_start;
	int64_t sample_end;

	int64_t sample_rate_num;
	int64_t sample_rate_div;




	TimeStore();
	//duration 1 sample
	TimeStore(int64_t Clock_time_base, int64_t startTime, int64_t sampleRateNum, int64_t sampleRateDiv);

	TimeStore(int64_t Clock_time_base, int64_t startTime, int64_t endTime, int64_t sampleRateNum, int64_t sampleRateDiv);
	TimeStore get_time_in_samplerate(int64_t sample_rate_num, int64_t sample_rate_div) const;

	TimeStore increment_with_frames(int64_t frames) const;
};



struct TimedTaskImpl;



class TimedTask
{

private:
	std::unique_ptr< TimedTaskImpl> impl;
	TimedTask() = delete;
	TimedTask(TimedTask&&) = delete;
	TimedTask(const TimedTask&) = delete;
public:
	TimedTask(const int64_t start_time, const int64_t periodN, const int64_t periodD, std::function<int64_t()> Get_time, int64_t Time_base);
	~TimedTask();
	//timeout in milliseconds, other values in 100ns units
	wait_t wait_for_signal(uint32_t timeout, TimeStore& ts);
};


class TimedTaskHighRes
{

private:
	std::unique_ptr< TimedTaskImpl> impl;
	TimedTaskHighRes() = delete;
public:
	TimedTaskHighRes(const int64_t start_time, const int64_t periodN, const int64_t periodD);
	~TimedTaskHighRes();
	//timeout in milliseconds, other values in 100ns units
	wait_t wait_for_signal(uint32_t timeout, TimeStore& ts);
};



class TimedTaskAstronomic
{

private:
	std::unique_ptr< TimedTaskImpl> impl;
	TimedTaskAstronomic() = delete;
public:
	TimedTaskAstronomic(const int64_t start_time, const int64_t periodN, const int64_t periodD);
	~TimedTaskAstronomic();
	//timeout in milliseconds, other values in 100ns units
	wait_t wait_for_signal(uint32_t timeout, TimeStore& ts);
};

