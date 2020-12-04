#include "LibLogging.h"
#include "LoggingClass.h"
#include "../Common/ThirdParty/nlohmann_json/src/json.hpp"
#include "../Common/Clock/Clock.h"

namespace Log
{
	void SetInstanceId(const std::string & instanceId)
	{
		CLogger::SetInstanceId(instanceId);
	}
	void SetLogLevel(LOGLEVEL level)
	{
		CLogger::SetLogLevel(level);
	}
	void SetAsyncLogging(bool enable)
	{
		CLogger::SetAsyncLogging(enable);
	}
	void SetODSLogging(bool enable)
	{
		CLogger::SetODSLogging(enable);
	}
	void SetFileLogging(bool enable)
	{
		CLogger::SetFileLogging(enable);
	}
	bool GetFileLogging()
	{
		return CLogger::GetFileLogging();
	}
	void SetConsoleLogging(bool enable)
	{
		CLogger::SetConsoleLogging(enable);
	}

	void LogEvent(const LOGLEVEL level, const LOGTYPE logtype, const char* submoduleName, const char* text)
	{
		try
		{
			return CLogger::LogEvent(level, logtype, submoduleName, __sys_user_name__, text);
		}
		catch (...)
		{
			OutputDebugStringA("[LOG::LogEvent EXCEPTION]");
		}
	}


	void LogEvent(const LOGLEVEL level, const LOGTYPE logtype, const char* submoduleName, const char* text, nlohmann::json& data)
	{
		try
		{
			return CLogger::LogEvent(level, logtype, submoduleName, __sys_user_name__, text, data);
		}
		catch (...)
		{
			OutputDebugStringA("[LOG::LogEvent EXCEPTION]");
		}
	}

	struct PerformanceLog::IMPL
	{
		std::string _name;
		int64_t _show_if_bigger_100ns;
		int64_t _number_of_iterations;
		int64_t start;
		PerformanceLog::IMPL(const std::string& name, int64_t number_of_iterations, int64_t show_if_bigger_100ns) :_name(name),
			_show_if_bigger_100ns(show_if_bigger_100ns), start(ClockHighRes::get_time()),
			_number_of_iterations(number_of_iterations<1? 1:number_of_iterations)
		{

		};
	};


	PerformanceLog::PerformanceLog(const std::string& name, int64_t numberOfIterations, int64_t show_if_bigger_100ns) :
		_impl( new PerformanceLog::IMPL(name, numberOfIterations, show_if_bigger_100ns))
	{

	}
	PerformanceLog::PerformanceLog(const std::string& name, int64_t show_if_bigger_100ns) :_impl(new PerformanceLog::IMPL(name, 1, show_if_bigger_100ns))
	{
	}
	PerformanceLog::~PerformanceLog()
	{
		if (_impl) {
			try
			{
				auto _100nanos =   ClockHighRes::get_time() - _impl->start;
				if (_100nanos >= _impl->_show_if_bigger_100ns)
				{
					if (_impl->_number_of_iterations > 1)
					{
						LogEvent(LOGLEVEL::Performance, LOGTYPE::System, _impl->_name.c_str(), fmt::format("{} ms per iter ({})", (double)_100nanos/(10000.0*_impl->_number_of_iterations), _impl->_number_of_iterations).c_str());
					}
					else
					{
						LogEvent(LOGLEVEL::Performance, LOGTYPE::System, _impl->_name.c_str(), fmt::format("{} ms", (double)_100nanos/10000.0).c_str());
					}
				}
			}
			catch (...)
			{

			}
			delete _impl;
		}
	}
}
