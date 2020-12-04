#pragma once

#if  (defined(WIN32) || defined(WIN32_) || defined(WINDOWS))
#include <Windows.h>

#endif

#include <fmt/format.h>

#include <string>
#include <time.h>
#include <sstream>
#include <iomanip>
#include <chrono>


#pragma warning(disable:4505) 
struct tm_ext {
	int year;
	int month;
	int day;

	int hour;
	int minutes;
	int seconds;

	int milliseconds;
	int microseconds;
	int nanoseconds;
};


inline bool gmtime_mp(const time_t* tt, tm* tm_) {
#ifdef __linux__
	return gmtime_r(tt, tm_) != nullptr;
#else
	return gmtime_s(tm_, tt) == 0;
#endif

}


namespace sunny
{
	constexpr inline bool isDigit(char C)
	{
		return  ((C >= 0x30) && (C <= 0x39));
	}
}

static tm_ext get_system_time() {
	tm_ext te{};
	std::chrono::time_point<std::chrono::system_clock> t = std::chrono::system_clock::now();

	auto since_epoch = t.time_since_epoch();
	std::chrono::seconds s = std::chrono::duration_cast<std::chrono::seconds>(since_epoch);
	since_epoch -= s;

	auto nano = std::chrono::duration_cast<std::chrono::nanoseconds>(since_epoch).count();


	time_t time = s.count();
	tm tmpTime;
	if (!gmtime_mp(&time, &tmpTime)) return te;


	te.year = tmpTime.tm_year + 1900;
	te.month = tmpTime.tm_mon + 1;
	te.day = tmpTime.tm_mday;
	te.hour = tmpTime.tm_hour;
	te.minutes = tmpTime.tm_min;
	te.seconds = tmpTime.tm_sec;

	te.milliseconds = static_cast<int>(nano / 1000000ll);
	nano -= (te.milliseconds * 1000000ll);
	te.microseconds = static_cast<int>(nano / 1000ll);
	nano -= (te.microseconds * 1000ll);
	te.nanoseconds = static_cast<int>(nano);
	return te;
}


static tm_ext get_local_time() {
	tm_ext te{};
	std::chrono::time_point<std::chrono::system_clock> t = std::chrono::system_clock::now();

	auto since_epoch = t.time_since_epoch();
	std::chrono::seconds s = std::chrono::duration_cast<std::chrono::seconds>(since_epoch);
	since_epoch -= s;

	auto nano = std::chrono::duration_cast<std::chrono::nanoseconds>(since_epoch).count();


	time_t time = s.count();
	tm tmpTime;
	if (0 != localtime_s(&tmpTime, &time)) return te;


	te.year = tmpTime.tm_year + 1900;
	te.month = tmpTime.tm_mon + 1;
	te.day = tmpTime.tm_mday;
	te.hour = tmpTime.tm_hour;
	te.minutes = tmpTime.tm_min;
	te.seconds = tmpTime.tm_sec;

	te.milliseconds = static_cast<int>(nano / 1000000ll);
	nano -= (te.milliseconds * 1000000ll);
	te.microseconds = static_cast<int>(nano / 1000ll);
	nano -= (te.microseconds * 1000ll);
	te.nanoseconds = static_cast<int>(nano);
	return te;
}


static tm_ext get_local_time(int64_t _100ns) {
	tm_ext te{};
	//std::chrono::time_point<std::chrono::system_clock> t = std::chrono::system_clock::now();


	lldiv_t ll = lldiv(_100ns, 10000000ll);
	time_t time = ll.quot;
	int64_t ns = ll.rem;
	tm tmpTime;
	//if (!gmtime_mp(&time, &tmpTime)) return te;
	if (0 != localtime_s(&tmpTime, &time)) return te;

	te.year = tmpTime.tm_year + 1900;
	te.month = tmpTime.tm_mon + 1;
	te.day = tmpTime.tm_mday;
	te.hour = tmpTime.tm_hour;
	te.minutes = tmpTime.tm_min;
	te.seconds = tmpTime.tm_sec;

	te.milliseconds = static_cast<int>(ns / 10000ll);
	ns -= (te.milliseconds * 10000ll);
	te.microseconds = static_cast<int>(ns / 10ll);
	ns -= (te.microseconds * 10ll);
	te.nanoseconds = static_cast<int>(ns * 100);
	return te;

}


static tm_ext get_system_time(int64_t _100ns) {
	tm_ext te{};
	//std::chrono::time_point<std::chrono::system_clock> t = std::chrono::system_clock::now();


	lldiv_t ll = lldiv(_100ns, 10000000ll);
	time_t time = ll.quot;
	int64_t ns = ll.rem;
	tm tmpTime;
	if (!gmtime_mp(&time, &tmpTime)) return te;


	te.year = tmpTime.tm_year + 1900;
	te.month = tmpTime.tm_mon + 1;
	te.day = tmpTime.tm_mday;
	te.hour = tmpTime.tm_hour;
	te.minutes = tmpTime.tm_min;
	te.seconds = tmpTime.tm_sec;

	te.milliseconds = static_cast<int>(ns / 10000ll);
	ns -= (te.milliseconds * 10000ll);
	te.microseconds = static_cast<int>(ns / 10ll);
	ns -= (te.microseconds * 10ll);
	te.nanoseconds = static_cast<int>(ns * 100);
	return te;

}

static tm_ext get_system_time(std::chrono::time_point<std::chrono::system_clock> t) {
	tm_ext te{};
	//std::chrono::time_point<std::chrono::system_clock> t = std::chrono::system_clock::now();

	auto since_epoch = t.time_since_epoch();
	std::chrono::seconds s = std::chrono::duration_cast<std::chrono::seconds>(since_epoch);
	since_epoch -= s;

	auto nano = std::chrono::duration_cast<std::chrono::nanoseconds>(since_epoch).count();


	time_t time = s.count();
	tm tmpTime;
	if (!gmtime_mp(&time, &tmpTime)) return te;


	te.year = tmpTime.tm_year + 1900;
	te.month = tmpTime.tm_mon + 1;
	te.day = tmpTime.tm_mday;
	te.hour = tmpTime.tm_hour;
	te.minutes = tmpTime.tm_min;
	te.seconds = tmpTime.tm_sec;

	te.milliseconds = static_cast<int>(nano / 1000000ll);
	nano -= (te.milliseconds * 1000000ll);
	te.microseconds = static_cast<int>(nano / 1000ll);
	nano -= (te.microseconds * 1000ll);
	te.nanoseconds = static_cast<int>(nano);
	return te;

}

#if  (defined(WIN32) || defined(WIN32_) || defined(WINDOWS))
static time_t TimeFromSystemTime(const SYSTEMTIME* pTime)
{
	struct tm tm;
	memset(&tm, 0, sizeof(tm));

	tm.tm_year = pTime->wYear;
	tm.tm_mon = pTime->wMonth - 1;
	tm.tm_mday = pTime->wDay;

	tm.tm_hour = pTime->wHour;
	tm.tm_min = pTime->wMinute;
	tm.tm_sec = pTime->wSecond;

	return mktime(&tm);
}

static std::string SYSTEMTIMEToISOString(const SYSTEMTIME& time)
{
	//YYYY-MM-DDTHH:mm:ss.sssZ
	return  fmt::format("{:04}-{:02}-{:02}T{:02}:{:02}:{:02}.{:03}Z", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);
}

static std::string SYSTEMTIMEToTimeString(const SYSTEMTIME& time)
{
	return  fmt::format("{:02}:{:02}:{:02}", time.wHour, time.wMinute, time.wSecond);
}

//
// use utf8_to_wstring(SYSTEMTIMEToTimeString(time)) instead
//static std::wstring SYSTEMTIMEToTimeStringW(const SYSTEMTIME& time)
//{
//	return  utf8_to_wstring(SYSTEMTIMEToTimeString(time));
//}

static std::string SYSTEMTIMEToDateString(const SYSTEMTIME& time)
{
	if (time.wYear == 0) return "0000-00-00";
	return  fmt::format("{:04}-{:02}-{:02}", time.wYear, time.wMonth, time.wDay);
}

//
// use utf8_to_wstring(SYSTEMTIMEToDateString(time)) instead
//static std::wstring SYSTEMTIMEToDateStringW(const SYSTEMTIME& time)
//{
//	return  utf8_to_wstring(SYSTEMTIMEToDateString(time));
//}
#endif


static inline std::string UTCtimeToISOString(time_t time) {
	//YYYY-MM-DDTHH:mm:ss.sssZ

	tm tmpTime;
	if (gmtime_mp(&time, &tmpTime)) {

		return fmt::format("{:04}-{:02}-{:02}T{:02}:{:02}:{:02}Z",
			tmpTime.tm_year + 1900,
			tmpTime.tm_mon + 1,
			tmpTime.tm_mday,
			tmpTime.tm_hour,
			tmpTime.tm_min,
			tmpTime.tm_sec);
	}


	return "INVALID DATE";
}

static inline std::string UTCtimeToString(time_t time, const char* format) {
	//YYYY-MM-DDTHH:mm:ss.sssZ
	std::stringstream ss;
	tm tmpTime;
	if (gmtime_mp(&time, &tmpTime)) {
		ss << std::put_time(&tmpTime, format); //<FIXME>
	}
	else {
		ss << "INVALID DATE";
	}
	return ss.str();
}


enum class TIME_STRING_TYPE {
	FullWithTimeZone_Milliseconds,
	FullWithTimeZone_Microseconds,
	FullWithTimeZone_Seconds,
	ShortWithTandZ,
	YYYYMMDDHHMMSS
};


static inline std::string UTC_asString_ISO8601(const timespec& tmspc,
	const TIME_STRING_TYPE format = TIME_STRING_TYPE::FullWithTimeZone_Milliseconds) {

	tm tme{};
	time_t secs = tmspc.tv_sec;

	if (!gmtime_mp(&secs, &tme)) return "";


	switch (format) {
	case TIME_STRING_TYPE::FullWithTimeZone_Milliseconds:
		return fmt::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:03}+00", tme.tm_year + 1900, tme.tm_mon + 1, tme.tm_mday, tme.tm_hour,
			tme.tm_min, tme.tm_sec, tmspc.tv_nsec / 1000000);
	case TIME_STRING_TYPE::FullWithTimeZone_Microseconds:
		return fmt::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:06}+00", tme.tm_year + 1900, tme.tm_mon + 1, tme.tm_mday, tme.tm_hour,
			tme.tm_min, tme.tm_sec, tmspc.tv_nsec / 1000);
	case TIME_STRING_TYPE::ShortWithTandZ:
		return fmt::format("{:04}-{:02}-{:02}T{:02}:{:02}:{:02}.{:03}Z", tme.tm_year + 1900, tme.tm_mon + 1, tme.tm_mday, tme.tm_hour,
			tme.tm_min, tme.tm_sec, tmspc.tv_nsec / 1000000);
	case TIME_STRING_TYPE::YYYYMMDDHHMMSS:
		return fmt::format("{:04}{:02}{:02}{:02}{:02}{:02}", tme.tm_year + 1900, tme.tm_mon + 1, tme.tm_mday, tme.tm_hour, tme.tm_min,
			tme.tm_sec);
	case TIME_STRING_TYPE::FullWithTimeZone_Seconds:
		return fmt::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}+00", tme.tm_year + 1900, tme.tm_mon + 1, tme.tm_mday, tme.tm_hour, tme.tm_min, tme.tm_sec);
	}
	return "";
}


static inline std::string UTC_asString_ISO8601(const tm_ext& tim,
	const TIME_STRING_TYPE format = TIME_STRING_TYPE::FullWithTimeZone_Milliseconds) {
	switch (format) {
	case TIME_STRING_TYPE::FullWithTimeZone_Milliseconds:
		return fmt::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:03}+00", tim.year, tim.month, tim.day, tim.hour,
			tim.minutes, tim.seconds, tim.milliseconds);
	case TIME_STRING_TYPE::FullWithTimeZone_Microseconds:
		return fmt::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:06}+00", tim.year, tim.month, tim.day, tim.hour,
			tim.minutes, tim.seconds, tim.milliseconds * 1000 + tim.microseconds);
	case TIME_STRING_TYPE::ShortWithTandZ:
		return fmt::format("{:04}-{:02}-{:02}T{:02}:{:02}:{:02}.{:03}Z", tim.year, tim.month, tim.day, tim.hour, tim.minutes,
			tim.seconds, tim.milliseconds);
	case TIME_STRING_TYPE::YYYYMMDDHHMMSS:
		return fmt::format("{:04}{:02}{:02}{:02}{:02}{:02}", tim.year, tim.month, tim.day, tim.hour, tim.minutes,
			tim.seconds);
	case TIME_STRING_TYPE::FullWithTimeZone_Seconds:
		return fmt::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}+00", tim.year, tim.month, tim.day, tim.hour,
			tim.minutes, tim.seconds);
	}
	return "";

}


static inline std::string
UTCNow_asString_ISO8601(const TIME_STRING_TYPE format = TIME_STRING_TYPE::FullWithTimeZone_Milliseconds) {
	auto time_ext = get_system_time();
	return UTC_asString_ISO8601(time_ext, format);
}




struct TM {
	uint32_t year;
	uint32_t month;
	uint32_t day;
	uint32_t hour;
	uint32_t minute;
	double seconds;
	int32_t tz_hours;
	uint32_t tz_minutes;
};


static inline bool parse_ISO8601_FULL(const std::string& iso_str, TM& tstamp) {
	tstamp.year = UINT32_MAX;
	tstamp.month = UINT32_MAX;
	tstamp.day = UINT32_MAX;
	tstamp.hour = UINT32_MAX;
	tstamp.minute = UINT32_MAX;
	tstamp.seconds = -1;
	tstamp.tz_hours = INT32_MAX;
	tstamp.tz_minutes = 0;
	int cnt = 0;

	enum class parse_state {
		START,
		YEAR,
		YEAR_SEPARATOR,
		MONTH,
		MONTH_SEPARATOR,
		DAY,
		DAY_SEPARATOR,
		HOUR,
		HOUR_SEPARATOR,
		MINUTE,
		MINUTE_SEPARATOR,
		SECONDS_INT,
		SECONDS_FLOAT_SEPARATOR,
		SECONDS_FLOAT_PART,
		TIMEZONE_SIGN,
		TIMEZONE_HOURS,
		TIMEZONE_SEPARATOR,
		TIMEZONE_MINUTES,
		DONE
	};

	auto state(parse_state::START);
	//"{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:06}+00"
	//YYYY-MM-DD HH:MM:SS.abcdef+00:00
	std::string test = "";
	test.reserve(128);
	for (size_t i = 0; i < iso_str.size(); i++) {
		char ch = iso_str[i];
		switch (state) {
		case parse_state::START: {
			if (i != 0) return false;
			state = parse_state::YEAR;
		}
		[[fallthrough]];
		case parse_state::YEAR: {
			if (test.size() > 4) return false;
			if (!sunny::isDigit(ch)) {
				if (test.size() != 4) return false;

				tstamp.year = static_cast<uint32_t>(atol(test.c_str()));
				state = parse_state::YEAR_SEPARATOR;

			}
			else {
				test += ch;
				break;
			}
		}
		[[fallthrough]];
		case parse_state::YEAR_SEPARATOR: {
			if (ch != '-') return false;
			test = "";
			state = parse_state::MONTH;
			break;
		}
		case parse_state::MONTH: {
			if (test.size() > 2) return false;
			if (!sunny::isDigit(ch)) {
				if (test.size() != 2) return false;
				tstamp.month = static_cast<uint32_t>(atol(test.c_str()));
				state = parse_state::MONTH_SEPARATOR;
			}
			else {
				test += ch;
				break;
			}
		}
	    [[fallthrough]];
		case parse_state::MONTH_SEPARATOR: {
			if (ch != '-') return false;
			test = "";
			state = parse_state::DAY;
			break;
		}
		case parse_state::DAY: {
			if (test.size() > 2) return false;
			if (!sunny::isDigit(ch)) {
				if (test.size() != 2) return false;
				tstamp.day = static_cast<uint32_t>(atol(test.c_str()));
				state = parse_state::DAY_SEPARATOR;
			}
			else {
				test += ch;
				break;
			}
		}
		[[fallthrough]];
		case parse_state::DAY_SEPARATOR: {
			if ((ch != ' ') && (ch != 'T')) return false;
			test = "";
			state = parse_state::HOUR;
			break;
		}
		case parse_state::HOUR: {
			if (test.size() > 2) return false;
			if (!sunny::isDigit(ch)) {
				if (test.size() != 2) return false;
				tstamp.hour = static_cast<uint32_t>(atol(test.c_str()));
				state = parse_state::HOUR_SEPARATOR;
			}
			else {
				test += ch;
				break;
			}
		}
		[[fallthrough]];
		case parse_state::HOUR_SEPARATOR: {
			if (ch != ':') return false;
			test = "";
			state = parse_state::MINUTE;
			break;
		}
		case parse_state::MINUTE: {
			if (test.size() > 2) return false;
			if (!sunny::isDigit(ch)) {
				if (test.size() != 2) return false;
				tstamp.minute = static_cast<uint32_t>(atol(test.c_str()));
				state = parse_state::MINUTE_SEPARATOR;
			}
			else {
				test += ch;
				break;
			}
		}
		[[fallthrough]];
		case parse_state::MINUTE_SEPARATOR: {
			if (ch != ':') return false;
			test = "";
			state = parse_state::SECONDS_INT;
			break;
		}
		case parse_state::SECONDS_INT:
			if (test.size() > 2) return false;
			if (!sunny::isDigit(ch)) {
				if (test.size() != 2) return false;
				state = parse_state::SECONDS_FLOAT_SEPARATOR;
			}
			else {
				test += ch;
				break;
			}
		[[fallthrough]];
		case parse_state::SECONDS_FLOAT_SEPARATOR:
			if (ch != '.') {
				tstamp.seconds = atof(test.c_str());
				test = "";
				if ((ch != '+') && (ch != '-')) return false;
				//this is timezone
				state = parse_state::TIMEZONE_SIGN;
				i--;
				break;
			}
			else {
				test += ch;
				cnt = 0;
				state = parse_state::SECONDS_FLOAT_PART;
				break;
			}
		case parse_state::SECONDS_FLOAT_PART:
			if (!sunny::isDigit(ch)) {
				tstamp.seconds = atof(test.c_str());
				test = "";
				state = parse_state::TIMEZONE_SIGN;
			}
			else {
				cnt++;
				if (cnt > 10)
				{
					return false;
				}
				test += ch;
				break;
			}
		[[fallthrough]];
		case parse_state::TIMEZONE_SIGN: {
			if (ch == 'Z') {
				tstamp.tz_hours = 0;
				tstamp.tz_minutes = 0;

				state = parse_state::DONE;
				break;
			}
			if ((ch != '+') && (ch != '-'))
			{
				return false;
			}
			if (ch == '-') tstamp.tz_hours = -1;
			if (ch == '+') tstamp.tz_hours = 1;
			state = parse_state::TIMEZONE_HOURS;
			break;
		}
		case parse_state::TIMEZONE_HOURS: {
			if (test.size() > 2) return false;
			if (!sunny::isDigit(ch)) {
				if (test.size() != 2) return false;
				tstamp.tz_hours *= static_cast<uint32_t>(atol(test.c_str()));
				state = parse_state::TIMEZONE_SEPARATOR;
			}
			else {
				test += ch;
				break;
			}
		}
		[[fallthrough]];
		case parse_state::TIMEZONE_SEPARATOR: {
			if (ch != ':') return false;
			test = "";
			state = parse_state::TIMEZONE_MINUTES;
			break;
		}
		case parse_state::TIMEZONE_MINUTES: {
			if (test.size() > 2) return false;
			if (!sunny::isDigit(ch)) {
				if (test.size() != 2) return false;
				tstamp.tz_minutes = static_cast<uint32_t>(atol(test.c_str()));
				state = parse_state::TIMEZONE_SEPARATOR;
			}
			else {
				test += ch;
				break;
			}
		}
										  [[fallthrough]];
		case parse_state::DONE: break;
		default:
			return false;//
		}
	}

	if (!test.empty()) {
		if (state == parse_state::TIMEZONE_HOURS) {
			tstamp.tz_hours *= static_cast<uint32_t>(atol(test.c_str()));
		}
		else if (state == parse_state::TIMEZONE_MINUTES) {
			tstamp.tz_minutes = static_cast<uint32_t>(atol(test.c_str()));
		}
	}


	if (tstamp.year > 9999) return false;
	if (tstamp.month > 12) return false;
	if (tstamp.day > 31) return false;
	if (tstamp.hour > 23) return false;
	if (tstamp.minute > 59) return false;
	if (tstamp.seconds >= 60) return false;
	if ((tstamp.tz_hours > 23) || (tstamp.tz_hours < -23)) return false;
	if ((tstamp.tz_minutes > 59)) return false;
	return true;

}


static inline bool parse_ISO8601_ShortWithTandZ(const std::string& iso_str, TM& tstamp) {


	tstamp.year = UINT32_MAX;
	tstamp.month = UINT32_MAX;
	tstamp.day = UINT32_MAX;
	tstamp.hour = UINT32_MAX;
	tstamp.minute = UINT32_MAX;
	tstamp.seconds = -1;
	tstamp.tz_hours = INT32_MAX;
	tstamp.tz_minutes = 0;

	//"{:04}{:02}{:02}T{:02}{:02}{:02}{:03}Z"
	//YYYY-MM-DDTHH:MM:SS.123Z
	if (iso_str.size() != 24) return false;
	if (iso_str[10] != 'T') return false;
	if (iso_str.back() != 'Z') return false;


	try {

		tstamp.year = static_cast<uint32_t>(std::stoul(iso_str.substr(0, 4)));
		tstamp.month = static_cast<uint32_t>(std::stoul(iso_str.substr(5, 2)));
		tstamp.day = static_cast<uint32_t>(std::stoul(iso_str.substr(8, 2)));
		tstamp.hour = static_cast<uint32_t>(std::stoul(iso_str.substr(11, 2)));
		tstamp.minute = static_cast<uint32_t>(std::stoul(iso_str.substr(14, 2)));
		std::string sec = iso_str.substr(17, 2) + "." + iso_str.substr(20, 3) + "000";

		tstamp.seconds = std::stof(sec);

	}
	catch (...) {
		return false;
	}


	if (tstamp.year > 9999) return false;
	if (tstamp.month > 12) return false;
	if (tstamp.day > 31) return false;
	if (tstamp.hour > 23) return false;
	if (tstamp.minute > 59) return false;
	if (tstamp.seconds >= 60) return false;
	tstamp.tz_hours = 0;
	tstamp.tz_minutes = 0;
	return true;
}


static inline bool parse_ISO8601_YYYYMMDDHHMMSS(const std::string& iso_str, TM& tstamp) {
	tstamp.year = UINT32_MAX;
	tstamp.month = UINT32_MAX;
	tstamp.day = UINT32_MAX;
	tstamp.hour = UINT32_MAX;
	tstamp.minute = UINT32_MAX;
	tstamp.seconds = -1;
	tstamp.tz_hours = INT32_MAX;
	tstamp.tz_minutes = 0;

	//"{:04}{:02}{:02}T{:02}{:02}{:02}{:03}Z"
	//YYYYMMDDTHHMMSS123Z
	if (iso_str.size() != 14) return false;

	try {

		tstamp.year = static_cast<uint32_t>(std::stoul(iso_str.substr(0, 4)));
		tstamp.month = static_cast<uint32_t>(std::stol(iso_str.substr(4, 2)));
		tstamp.day = static_cast<uint32_t>(std::stol(iso_str.substr(6, 2)));
		tstamp.hour = static_cast<uint32_t>(std::stol(iso_str.substr(8, 2)));
		tstamp.minute = static_cast<uint32_t>(std::stol(iso_str.substr(10, 2)));
		std::string sec = iso_str.substr(12, 2);

		tstamp.seconds = std::stof(sec);

	}
	catch (...) {
		return false;
	}


	if (tstamp.year > 9999) return false;
	if (tstamp.month > 12) return false;
	if (tstamp.day > 31) return false;
	if (tstamp.hour > 23) return false;
	if (tstamp.minute > 59) return false;
	if (tstamp.seconds >= 60) return false;
	tstamp.tz_hours = 0;
	tstamp.tz_minutes = 0;
	return true;
}

static inline bool parse_ISO8601(const std::string& iso_str, TM& tstamp,
	const TIME_STRING_TYPE format = TIME_STRING_TYPE::FullWithTimeZone_Milliseconds) {
	try {
		switch (format) {
		case TIME_STRING_TYPE::FullWithTimeZone_Milliseconds:
		case TIME_STRING_TYPE::FullWithTimeZone_Seconds:
		case TIME_STRING_TYPE::FullWithTimeZone_Microseconds: {
			return parse_ISO8601_FULL(iso_str, tstamp);
		}
		case TIME_STRING_TYPE::ShortWithTandZ: {

			return parse_ISO8601_ShortWithTandZ(iso_str, tstamp);
		}
		case TIME_STRING_TYPE::YYYYMMDDHHMMSS: {
			return parse_ISO8601_YYYYMMDDHHMMSS(iso_str, tstamp);
		}
		}
	}
	catch (...) {

	}
	return false;


}

static inline bool is_valid_timestamp(const std::string& iso_str,
	const TIME_STRING_TYPE format = TIME_STRING_TYPE::FullWithTimeZone_Milliseconds) {
	TM t;
	bool res = parse_ISO8601(iso_str, t, format);;
	return res;
}

static inline std::string
UTC_Relative_asString_ISO8601(std::chrono::system_clock::duration duration /*Chrono system clock since epoch*/,
	const TIME_STRING_TYPE format = TIME_STRING_TYPE::FullWithTimeZone_Milliseconds) {
	auto since_epoch = std::chrono::system_clock::now().time_since_epoch() + duration;
	std::chrono::seconds s = std::chrono::duration_cast<std::chrono::seconds>(since_epoch);
	since_epoch -= s;

	std::chrono::milliseconds milli = std::chrono::duration_cast<std::chrono::milliseconds>(since_epoch);
	std::chrono::microseconds micro = std::chrono::duration_cast<std::chrono::microseconds>(since_epoch);

	time_t time = s.count();
	tm tmpTime;
	if (gmtime_mp(&time, &tmpTime)) {

		switch (format) {
		case TIME_STRING_TYPE::FullWithTimeZone_Seconds:
			return fmt::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}+00", tmpTime.tm_year + 1900,
				tmpTime.tm_mon + 1, tmpTime.tm_mday, tmpTime.tm_hour, tmpTime.tm_min, tmpTime.tm_sec);
		case TIME_STRING_TYPE::FullWithTimeZone_Milliseconds:
			return fmt::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:03}+00", tmpTime.tm_year + 1900,
				tmpTime.tm_mon + 1, tmpTime.tm_mday, tmpTime.tm_hour, tmpTime.tm_min, tmpTime.tm_sec,
				milli.count());
		case TIME_STRING_TYPE::FullWithTimeZone_Microseconds:
			return fmt::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:06}+00", tmpTime.tm_year + 1900,
				tmpTime.tm_mon + 1, tmpTime.tm_mday, tmpTime.tm_hour, tmpTime.tm_min, tmpTime.tm_sec,
				micro.count());
		case TIME_STRING_TYPE::ShortWithTandZ:
			return fmt::format("{:04}{:02}{:02}T{:02}{:02}{:02}{:03}Z", tmpTime.tm_year + 1900, tmpTime.tm_mon + 1,
				tmpTime.tm_mday, tmpTime.tm_hour, tmpTime.tm_min, tmpTime.tm_sec, milli.count());
		case TIME_STRING_TYPE::YYYYMMDDHHMMSS:
			return fmt::format("{:04}{:02}{:02}{:02}{:02}{:02}", tmpTime.tm_year + 1900, tmpTime.tm_mon + 1,
				tmpTime.tm_mday, tmpTime.tm_hour, tmpTime.tm_min, tmpTime.tm_sec);
		default:
			break;
		}
	}
	return "";
}
#pragma warning(default:4505) 