#pragma once
#include "LoggingTypes.h"
#include <string>
#include <fmt/format.h>

namespace Log
{
	void SetInstanceId(const std::string & instanceId);
	void SetLogLevel(LOGLEVEL level);
	void SetAsyncLogging(bool enable);
	void SetODSLogging(bool enable);
	void SetFileLogging(bool enable);
	bool GetFileLogging();

	void SetConsoleLogging(bool enable);

	void LogEvent(const LOGLEVEL level, const LOGTYPE logtype, const std::string& submoduleName, const std::string& text);
	void LogEventToFileSync(const LOGLEVEL level, const LOGTYPE logtype, const std::string& submoduleName, const std::string& text);

	inline void debug(const std::string & submodule, const std::string & message)
	{
		LogEvent(LOGLEVEL::Debug, LOGTYPE::System, submodule, message);
	};

	inline void info(const std::string & submodule, const std::string & message)
	{
		LogEvent(LOGLEVEL::Info, LOGTYPE::System, submodule, message);
	};

	inline void warn(const std::string & submodule, const std::string & message)
	{
		LogEvent(LOGLEVEL::Warn, LOGTYPE::System, submodule, message);
	};

	inline void error(const std::string & submodule, const std::string & message)
	{
		LogEvent(LOGLEVEL::Error, LOGTYPE::System, submodule, message);
	};

	inline void fatal(const std::string & submodule, const std::string & message)
	{
		LogEvent(LOGLEVEL::Fatal, LOGTYPE::System, submodule, message);
	};

	inline void performance(const std::string & submodule, const std::string & message)
	{
		LogEvent(LOGLEVEL::Performance, LOGTYPE::System, submodule, message);
	};

	template <typename... Ts>
	inline void debug(const std::string & submodule, const std::string & format_text, Ts... tail)
	{
		LogEvent(LOGLEVEL::Debug, LOGTYPE::System, submodule, fmt::format(format_text, tail...));
	};


	template <typename... Ts>
	inline void info(const std::string & submodule, const std::string & format_text, Ts... tail)
	{
		LogEvent(LOGLEVEL::Info, LOGTYPE::System, submodule, fmt::format(format_text, tail...));
	};

	template <typename... Ts>
	inline void warn(const std::string & submodule, const std::string & format_text, Ts... tail)
	{
		LogEvent(LOGLEVEL::Warn, LOGTYPE::System, submodule, fmt::format(format_text, tail...));
	};

	template <typename... Ts>
	inline void error(const std::string & submodule, const std::string & format_text, Ts... tail)
	{
		LogEvent(LOGLEVEL::Error, LOGTYPE::System, submodule, fmt::format(format_text, tail...));
	};

	template <typename... Ts>
	inline void fatal(const std::string & submodule, const std::string & format_text, Ts... tail)
	{
		LogEvent(LOGLEVEL::Fatal, LOGTYPE::System, submodule, fmt::format(format_text, tail...));
	};

	template <typename... Ts>
	inline void performance(const std::string & submodule, const std::string & format_text, Ts... tail)
	{
		LogEvent(LOGLEVEL::Performance, LOGTYPE::System, submodule, fmt::format(format_text, tail...));
	};

	class PerformanceLog
	{
	private:
		struct IMPL;
		IMPL* _impl = nullptr;
		PerformanceLog() = delete;
		PerformanceLog(PerformanceLog&&) = delete;
		PerformanceLog(const PerformanceLog&) = delete;
	public:
		PerformanceLog(const std::string & name, int64_t show_if_bigger_100ns = 0);
		PerformanceLog(const std::string& name, int64_t numberOfIterations, int64_t show_if_bigger_100ns);
		~PerformanceLog();
	};

}