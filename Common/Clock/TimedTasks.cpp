#include "TimedTasks.h"
#include <algorithm>
#include "../math_utils.h"

#include <atomic>

#ifndef NOMINMAX
#error "DEFINE NOMINMAX"
#endif
#include <Windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

TimeStore::TimeStore()

	:clock_time_base(0)
	, start_time(0)
	, actual_time(0)
	, event_time_start(0)
	, event_time_end(0)

	, sample_start(0)
	, sample_end(0)

	, sample_rate_num(1)
	, sample_rate_div(1)
{
}

TimeStore::TimeStore(int64_t Clock_time_base, int64_t startTime, int64_t sampleRateNum, int64_t sampleRateDiv) :clock_time_base(Clock_time_base)
{
	sample_rate_num = sampleRateNum;
	sample_rate_div = sampleRateDiv;

	int64_t div = clock_time_base * sample_rate_div;

	start_time = 0;

	sample_start = DivMulRound(startTime - start_time, div, sample_rate_num);
	sample_end = sample_start + 1;



	event_time_start = DivMulRound(sample_start, sample_rate_num, div) + start_time;
	event_time_end = DivMulRound(sample_end, sample_rate_num, div) + start_time;

	actual_time = event_time_start;
}

TimeStore::TimeStore(int64_t Clock_time_base, int64_t startTime, int64_t endTime, int64_t sampleRateNum, int64_t sampleRateDiv) :clock_time_base(Clock_time_base)
{
	sample_rate_num = sampleRateNum;
	sample_rate_div = sampleRateDiv;

	int64_t div = clock_time_base * sample_rate_div;

	start_time = 0;

	sample_start = DivMulRound(startTime - start_time, div, sample_rate_num);
	sample_end = DivMulRound(endTime - start_time, div, sample_rate_num);



	event_time_start = DivMulRound(sample_start, sample_rate_num, div) + start_time;
	event_time_end = DivMulRound(sample_end, sample_rate_num, div) + start_time;

	actual_time = event_time_start;
}

TimeStore TimeStore::get_time_in_samplerate(int64_t sample_rate_num, int64_t sample_rate_div) const
{
	if (clock_time_base == 0) throw std::exception("ClockType TimeBase is wrong");

	TimeStore new_time = *this;
	int64_t div = clock_time_base * sample_rate_div;


	new_time.sample_start = DivMulRound(event_time_start - start_time, div, sample_rate_num);
	new_time.sample_end = DivMulRound(event_time_end - start_time, div, sample_rate_num);

	new_time.sample_rate_num = sample_rate_num;
	new_time.sample_rate_div = sample_rate_div;

	new_time.event_time_start = DivMulRound(new_time.sample_start, sample_rate_num, div) + start_time;
	new_time.event_time_end = DivMulRound(new_time.sample_end, sample_rate_num, div) + start_time;
	return new_time;
}


TimeStore TimeStore::increment_with_frames(int64_t frames) const
{
	if (clock_time_base == 0) throw std::exception("ClockType TimeBase is wrong");

	TimeStore ts = *this;

	int64_t rela_current_time = ts.event_time_start - ts.start_time;
	int64_t frame_no = DivMulRound(rela_current_time, ts.sample_rate_div * clock_time_base, ts.sample_rate_num);
	frame_no += frames;
	ts.sample_start = frame_no;
	ts.sample_end = frame_no + 1;
	ts.event_time_start = ts.start_time + DivMulRound(ts.sample_start, ts.sample_rate_num, ts.sample_rate_div * clock_time_base);
	ts.event_time_end = ts.start_time + DivMulRound(ts.sample_end, ts.sample_rate_num, ts.sample_rate_div * clock_time_base);
	return ts;
}




struct TimedTaskImpl
{
private:

	std::function<int64_t()>  get_time;
	wait_t wait_timeout(int64_t absolute_value);
	std::atomic<bool> abort_value;
public:
	int64_t     _start_time;

	int64_t     _period_num;
	int64_t     _period_div;

	int64_t     _current_time;

	int64_t time_base;
	TimedTaskImpl(const int64_t start_time, const int64_t periodN, const int64_t periodD, std::function<int64_t()>  Get_time, int64_t Time_base);
	~TimedTaskImpl();
	wait_t wait_for_signal(uint32_t timeout, TimeStore& ts);
	void abort();
};







TimedTaskImpl::TimedTaskImpl(const int64_t start_time, const int64_t periodN, const int64_t periodD, std::function<int64_t()>  Get_time, int64_t Time_base) :
	get_time(Get_time), abort_value(false), time_base(Time_base)
{

	auto now = get_time();


	_period_div = periodD;
	_period_num = periodN;


	_start_time = start_time;

	_current_time = start_time;


	_start_time = 0;
	_current_time = std::max(now, start_time);


	int64_t rela_current_time = _current_time - _start_time;
	int64_t frame_no = DivMulRound(rela_current_time, _period_div * time_base, _period_num);
	frame_no++;

	int64_t tm = _start_time + DivMulRound(frame_no, _period_num, _period_div * time_base);
	_current_time = tm;
	TIMECAPS tc;
	timeGetDevCaps(&tc, sizeof(TIMECAPS));
	auto tbRes = timeBeginPeriod(std::max(tc.wPeriodMin, 1u));
	if (TIMERR_NOERROR != tbRes) //2ms minimum
	{
		//Something is wrong
		throw std::exception("Time Begin Period Failed");
		return;
	}
}

TimedTaskImpl::~TimedTaskImpl()
{
	abort();
}

wait_t TimedTaskImpl::wait_timeout(int64_t absolute_value)
{
	auto now = get_time();
	constexpr auto milli_period = 2ll;
	int64_t two_milliseconds = ((milli_period * time_base) / 1000ll);

	if ((absolute_value - now) < two_milliseconds) return wait_t::event;



	while (!abort_value.load())
	{
		int64_t nextMsPeriod = (absolute_value - now) / (time_base / 1000);
		std::this_thread::sleep_for(std::chrono::milliseconds(nextMsPeriod / 2));
		now = get_time();
		if ((absolute_value - now) <= two_milliseconds) return wait_t::event;
	}
	return wait_t::aborted;
}

wait_t TimedTaskImpl::wait_for_signal(uint32_t timeout, TimeStore& ts)
{

	auto wait_abs_time = std::min(_current_time, get_time() + (static_cast<int64_t>( timeout) * time_base) / 1000ll);

	auto res = wait_timeout(wait_abs_time);
	if (res != wait_t::event) return res;
	if (wait_abs_time < _current_time) return wait_t::timeout;


	auto milli_tb = time_base / 1000ll;

	int64_t rela_current_time = _current_time - _start_time;
	int64_t frame_no = DivMulRound(rela_current_time, _period_div * time_base, _period_num);
	frame_no++;

	int64_t endtime = _start_time + DivMulRound(frame_no, _period_num, _period_div * time_base);


	ts.actual_time = get_time();
	ts.clock_time_base = time_base;
	ts.event_time_start = _current_time;
	ts.event_time_end = endtime;

	ts.start_time = _start_time;
	int64_t div = time_base * _period_div;
	ts.sample_start = DivMulRound(ts.event_time_start - ts.start_time, div, _period_num);
	ts.sample_end = DivMulRound(ts.event_time_end - ts.start_time, div, _period_num);
	ts.sample_rate_div = _period_div;
	ts.sample_rate_num = _period_num;

	_current_time = endtime;


	return wait_t::event;
}

void TimedTaskImpl::abort()
{
	abort_value.store(true);
}



TimedTaskHighRes::TimedTaskHighRes(const int64_t start_time, const int64_t periodN, const int64_t periodD) :
	impl(std::make_unique<TimedTaskImpl>(start_time, periodN, periodD, ClockHighRes::get_time_value, ClockHighRes::get_time_base()))
{
}

TimedTaskHighRes::~TimedTaskHighRes()
{
	impl = nullptr;
}

wait_t TimedTaskHighRes::wait_for_signal(uint32_t timeout, TimeStore& ts)
{
	return impl->wait_for_signal(timeout, ts);
}

TimedTaskAstronomic::TimedTaskAstronomic(const int64_t start_time, const int64_t periodN, const int64_t periodD) :
	impl(std::make_unique<TimedTaskImpl>(start_time, periodN, periodD, ClockAstronomic::get_time_value, ClockAstronomic::get_time_base()))
{
}

TimedTaskAstronomic::~TimedTaskAstronomic()
{
	impl = nullptr;
}

wait_t TimedTaskAstronomic::wait_for_signal(uint32_t timeout, TimeStore& ts)
{
	return impl->wait_for_signal(timeout, ts);
}


TimedTask::TimedTask(const int64_t start_time, const int64_t periodN, const int64_t periodD, std::function<int64_t()>  Get_time, int64_t Time_base):
	impl(std::make_unique<TimedTaskImpl>(start_time, periodN, periodD, Get_time, Time_base))
{
}

TimedTask::~TimedTask()
{
	impl = nullptr;
}

wait_t TimedTask::wait_for_signal(uint32_t timeout, TimeStore& ts)
{
	return impl->wait_for_signal(timeout, ts);
}
