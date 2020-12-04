#pragma once
#include <list>
#include <stdint.h>
#include "../Events.h"
#include <chrono>
#include <algorithm>
#include <type_traits>




enum class ClockType_t
{
	Astronomic,
	HighResolution,
	Unknown
};




namespace ClockHighRes
{

	
	[[nodiscard]] constexpr int64_t get_time_base()
	{
		return 10000000ll;
	}

	template<class _Rep, class _Period>
	[[nodiscard]] inline int64_t get_time(::std::chrono::duration<_Rep, _Period> increment)
	{
		std::chrono::nanoseconds j =std::chrono::high_resolution_clock::now().time_since_epoch() + increment;
		return j.count() / 100;
	}

	[[nodiscard]] inline int64_t get_time(int64_t _100nsIncrement)
	{
		
		return get_time(std::chrono::nanoseconds(_100nsIncrement * 100));
	}

	[[nodiscard]] inline int64_t get_time()
	{

		return get_time(std::chrono::nanoseconds(0));
	}

	[[nodiscard]] inline int64_t get_time_value()
	{

		return get_time(std::chrono::nanoseconds(0));
	}
}



namespace ClockAstronomic
{

	

	[[nodiscard]] constexpr int64_t get_time_base()
	{
		return 10000000ll;
	}

	template<class _Rep, class _Period>
	[[nodiscard]] inline int64_t get_time(::std::chrono::duration<_Rep, _Period> increment)
	{
		std::chrono::nanoseconds j = std::chrono::system_clock::now().time_since_epoch() + increment;
		return j.count() / 100;
	}

	[[nodiscard]] inline int64_t get_time(int64_t _100nsIncrement)
	{
		return get_time(std::chrono::nanoseconds(_100nsIncrement * 100));
	}


	[[nodiscard]] inline int64_t get_time()
	{

		return get_time(std::chrono::nanoseconds(0));
	}

	[[nodiscard]] inline int64_t get_time_value()
	{

		return get_time(std::chrono::nanoseconds(0));
	}
}


namespace Clock
{
	[[nodiscard]] static int64_t get_time_base(ClockType_t Type)
	{
		switch (Type)
		{
		case ClockType_t::Astronomic:
			return ClockAstronomic::get_time_base();
		case ClockType_t::HighResolution:
			return ClockHighRes::get_time_base();
		default:
			throw std::exception("Incompatible ClockType");
			break;
		}
	}



	template<class _Rep, class _Period>
	[[nodiscard]] int64_t get_time(ClockType_t Type, ::std::chrono::duration<_Rep, _Period> increment)
	{
		switch (Type)
		{
		case ClockType_t::Astronomic:
			return ClockAstronomic::get_time(increment);
		case ClockType_t::HighResolution:
			return ClockHighRes::get_time(increment);
		default:
			throw std::exception("Incompatible ClockType");
			break;
		}
	}



	[[nodiscard]] static int64_t get_time(ClockType_t Type, int64_t _100nsIncrement = 0)
	{
		return get_time(Type, std::chrono::nanoseconds(_100nsIncrement * 100));	
	}
}