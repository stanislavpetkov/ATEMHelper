#pragma once

#include "LoggingTypes.h"
#include <Windows.h>
#include <string>
#include <fstream>
#include <fmt/format.h>
#include "../Common/Events.h"

#include "../Common/time_utils.h"

#include "../Common/ThirdParty/nlohmann_json/src/json.hpp"


#include <array>
#include <chrono>
#include <mutex>
#include <concurrent_queue.h>
#include <thread>
#include "../Common/sys_utils.h"


struct OutputStreamStruct
{
	std::ofstream file;
	int day;

	OutputStreamStruct() : day(0) { file.close(); };
	~OutputStreamStruct() {
		if (file.is_open())
		{
			file.close();
		}
	}
};





namespace logger
{

	const std::wstring log_subfolder = L"\\PlayBoxNeo\\logs\\";

	namespace keys
	{
		const std::string timestamp = "timestamp";

		const std::string server_name = "server_name";
		const std::string thread_id = "thread_id";
		const std::string process_id = "process_id";

		const std::string level_txt = "log_level_txt";
		const std::string type_txt = "log_type_txt";

		const std::string module_binary = "module_binary";
		const std::string application = "application";
		const std::string module_instance = "module_instance";

		const std::string submodule = "submodule";

		const std::string username = "username";


		const std::string description = "description";
		const std::string data = "data";


		//const std::string level = "log_level";
		//const std::string type = "log_type";
	}




}


constexpr unsigned char smarker[3] = { 0xEF, 0xBB, 0xBF };

struct MESSAGE_DATA_t
{
	LOGTYPE log_type = LOGTYPE::System;
	size_t proc_id = 0;
	size_t thr_id = 0;
	LOGLEVEL log_level = LOGLEVEL::Debug;
	std::string level_text;
	std::string timestamp;
	std::string server_name;
	std::string type_text;
	std::string application;
	std::string module_bin;
	std::string module_inst;
	std::string submodule;
	std::string username;
	std::string description;
};




constexpr const char* __sys_user_name__ = u8"SYS_USER";

class CLogger
{
private:
	const std::string m_applicationName;
	const std::string m_moduleName;
	const std::string m_server_fqdn_name;

	std::string m_instanceId;


	bool useODSLogging;
	bool useConsoleLogging;
	bool useFileLogging;
	Event we_have_messages{ false,false };
	bool use_async_mode;
	bool exit_async;
	LOGLEVEL min_log_level;
	std::thread async_transmit;

	std::array<OutputStreamStruct, (uint32_t)LOGTYPE::MaxTypes> logFiles;

	concurrency::concurrent_queue<MESSAGE_DATA_t> dq;
	std::recursive_mutex mtx;

	void async_transmit_func()
	{
		if (GetThreadPriority(GetCurrentThread()) != THREAD_PRIORITY_IDLE)
		{
			SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_IDLE);
		}

		while (true)
		{
			MESSAGE_DATA_t md{};

			if (dq.try_pop(md))
			{
				dump_message(md);				
			}
			else
			{
				if (exit_async) return; //exit request;
				we_have_messages.wait(250);
			}
		}
	}

	void dump_message(const MESSAGE_DATA_t& md)
	{

		if (useODSLogging || useConsoleLogging || useFileLogging)
		{



#ifdef OutputDebugString
			if (useODSLogging) {




				auto message = pbn::to_wstring(fmt::format("[{}][{}] {} {} {} {}\n",
					m_applicationName,
					m_instanceId,
					md.level_text,
					md.thr_id,
					md.submodule,
					md.description));

				OutputDebugStringW(message.c_str());
			}
#endif

			if (useConsoleLogging) {

				auto message = fmt::format("{}\t{}\t{}\t{}\t{}\n",
					md.timestamp,
					md.level_text,
					md.thr_id,
					md.submodule,
					md.description);


				printf("%s", message.c_str());
				//fflush(stdout); <TODO> Performance??
			}



			if (useFileLogging) LogToFile(md);
		}
	}


	CLogger() :
		m_applicationName(pbn::fs::get_file_name_without_extension(pbn::OS::GetCurrentModuleFileName())),
		m_moduleName(pbn::OS::GetCurrentModuleFileName()),
		m_instanceId(""),
		m_server_fqdn_name(pbn::OS::get_computer_name()),
		useODSLogging(true),
		useConsoleLogging(true),
		useFileLogging(false),
		use_async_mode(false),
		exit_async(false),
		min_log_level(LOGLEVEL::Debug)
	{


	}

	CLogger(CLogger&&) = delete;
	CLogger(const CLogger&) = delete;
	CLogger& operator=(const CLogger&) = delete;
	CLogger& operator=(CLogger&&) = delete;

	~CLogger()
	{

		exit_async = true;
		we_have_messages.set_all();
		if (async_transmit.joinable())
		{
			async_transmit.join();
		}


		for (OutputStreamStruct& fs : logFiles)
		{
			if (fs.file.is_open())
			{
				fs.file.close();
			}
		}



	}

	static CLogger* GetInstance()
	{
		static CLogger myinstance;
		return &myinstance;
	}

	static inline bool HasLogDestinations(CLogger* inst)
	{
		return (inst->useODSLogging ||
			inst->useConsoleLogging || inst->useFileLogging);
	}
public:
	static void SetLogLevel(LOGLEVEL level)
	{
		CLogger* myinstance = CLogger::GetInstance();
		std::unique_lock lock(myinstance->mtx);
		myinstance->min_log_level = level;
	}

	static void SetAsyncLogging(bool enable)
	{
		CLogger* myinstance = CLogger::GetInstance();
		std::unique_lock lock(myinstance->mtx);

		if ((enable) && (!myinstance->use_async_mode))
		{
			myinstance->exit_async = false;
			myinstance->async_transmit = std::thread([] {
				CLogger* myinstance = CLogger::GetInstance();

				myinstance->async_transmit_func();
				});
		}

		if ((!enable) && (myinstance->use_async_mode))
		{
			myinstance->exit_async = true;
			myinstance->we_have_messages.set_all();
			if (myinstance->async_transmit.joinable())
			{
				myinstance->async_transmit.join();
			}
		}
		myinstance->use_async_mode = enable;

	}
	static void SetODSLogging(bool enable)
	{
		CLogger* myinstance = CLogger::GetInstance();
		std::unique_lock lock(myinstance->mtx);
		myinstance->useODSLogging = enable;

	}


	static void SetFileLogging(bool enable)
	{
		CLogger* myinstance = CLogger::GetInstance();
		std::unique_lock lock(myinstance->mtx);
		myinstance->useFileLogging = enable;

	}


	static bool GetFileLogging()
	{
		CLogger* myinstance = CLogger::GetInstance();
		std::unique_lock lock(myinstance->mtx);
		return myinstance->useFileLogging;
	}

	static std::string& GetInstanceId()
	{
		CLogger* myinstance = CLogger::GetInstance();
		std::unique_lock lock(myinstance->mtx);
		return myinstance->m_instanceId;
	}

	static void SetConsoleLogging(bool enable)
	{
		CLogger* myinstance = CLogger::GetInstance();
		std::unique_lock lock(myinstance->mtx);
		myinstance->useConsoleLogging = enable;

		//SetConsoleOutputCP(CP_UTF8);
		setlocale(LC_ALL, "C.UTF-8");

	}




	static void SetInstanceId(const std::string& insId)
	{
		CLogger* myinstance = CLogger::GetInstance();
		std::unique_lock lock(myinstance->mtx);
		//if (!myinstance->m_instanceId.empty()) return;
		myinstance->m_instanceId = insId;

		
	}



	void LogToFile(const MESSAGE_DATA_t& md)
	{
		std::unique_lock lock(mtx);


		OutputStreamStruct& ofs = logFiles[(uint32_t)md.log_type];
		auto& strm = ofs.file;


		using namespace std;
		using namespace std::chrono;
		typedef duration<int, ratio_multiply<hours::period, ratio<24> >::type> days;

		system_clock::time_point now = system_clock::now();
		system_clock::duration tp = now.time_since_epoch();
		days d = duration_cast<days>(tp);


		if (strm.is_open() && (ofs.day != d.count()))
		{
			strm.close();
		}

		uint32_t dataSize = 0;

		if (!strm.is_open())
		{
			std::string logFileName = UTCNow_asString_ISO8601(TIME_STRING_TYPE::YYYYMMDDHHMMSS) + "_" + md.type_text + "_" + m_moduleName + "_[" + std::to_string(md.proc_id) + "].log";

			pbn::fs::Path logPath(pbn::OS::GetProgramDataFolder());
			logPath.append(logger::log_subfolder).append(m_applicationName);

			if (!m_instanceId.empty())  logPath.append(m_instanceId);


			if (!pbn::fs::create_dirs(logPath))
			{
				OutputDebugStringA("Can not create folder for logging");
				useFileLogging = false;
				return;
			}

			logPath.append(logFileName);
			std::wstring full_logname = logPath;

			strm.open(full_logname, std::ios::ate | std::ios::binary);

			ofs.day = d.count();

			if (!strm.is_open())
			{
				strm.close();
				return;
			}


			dataSize = 3;
			strm.write((char*)&smarker[0], dataSize);
		}


		auto theText = fmt::format("{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\n",
			md.timestamp,
			md.level_text,
			md.type_text,
			md.application,
			md.module_bin,
			md.module_inst,
			md.proc_id,
			md.thr_id,
			md.submodule,
			md.description);

		dataSize = (uint32_t)theText.size();
		strm.write(theText.c_str(), dataSize);
		strm.flush();

		if (strm.tellp() >= MAX_LOG_FILE_SIZE)
		{
			strm.close();
		}
	}



	static void LogEvent(
		const LOGLEVEL level,
		const LOGTYPE logtype,
		const std::string& submoduleName,
		const std::string& userName,
		std::string text)
	{

		if (level >= LOGLEVEL::MaxLevels) return;
		if (logtype >= LOGTYPE::MaxTypes) return;


		CLogger* myinstance = CLogger::GetInstance();
		if (!HasLogDestinations(myinstance)) return;

		if ((int)level < (int)myinstance->min_log_level) return;

		std::string& instId(myinstance->GetInstanceId());



		uint32_t pid = (uint32_t)GetCurrentProcessId();
		uint32_t tid = (uint32_t)GetCurrentThreadId();

		MESSAGE_DATA_t md{};

		md.application = myinstance->m_applicationName;
		md.description = std::move(text);
		md.level_text = _log_levels_[static_cast<uint32_t>(level)];
		md.log_level = level;
		md.log_type = logtype;
		md.module_bin = myinstance->m_moduleName;
		md.module_inst = instId;
		md.proc_id = pid;
		md.server_name = myinstance->m_server_fqdn_name;
		md.submodule = submoduleName;;
		md.thr_id = tid;
		md.timestamp = UTCNow_asString_ISO8601(TIME_STRING_TYPE::FullWithTimeZone_Microseconds);


		md.type_text = (static_cast<uint32_t>(logtype) >= 0) && (static_cast<uint32_t>(logtype) < std::size(_log_types_)) ? _log_types_[static_cast<uint32_t>(logtype)] : "unknown";
		md.username = userName;

		if (!myinstance->use_async_mode)
		{
			std::unique_lock lock(myinstance->mtx);
			myinstance->dump_message(md);			
			return;
		}

		myinstance->dq.push(std::move(md));
		myinstance->we_have_messages.set_all();
	};



	static void LogEventToFile(
		const LOGLEVEL level,
		const LOGTYPE logtype,
		const std::string& submoduleName,
		const std::string& userName,
		std::string text)
	{

		if (level >= LOGLEVEL::MaxLevels) return;
		if (logtype >= LOGTYPE::MaxTypes) return;


		CLogger* myinstance = CLogger::GetInstance();
		if (!HasLogDestinations(myinstance)) return;

		if ((int)level < (int)myinstance->min_log_level) return;

		std::string& instId(myinstance->GetInstanceId());



		uint32_t pid = (uint32_t)GetCurrentProcessId();
		uint32_t tid = (uint32_t)GetCurrentThreadId();

		MESSAGE_DATA_t md{};

		md.application = myinstance->m_applicationName;
		md.description = std::move(text);
		md.level_text = _log_levels_[static_cast<uint32_t>(level)];
		md.log_level = level;
		md.log_type = logtype;
		md.module_bin = myinstance->m_moduleName;
		md.module_inst = instId;
		md.proc_id = pid;
		md.server_name = myinstance->m_server_fqdn_name;
		md.submodule = submoduleName;;
		md.thr_id = tid;
		md.timestamp = UTCNow_asString_ISO8601(TIME_STRING_TYPE::FullWithTimeZone_Microseconds);


		md.type_text = (static_cast<uint32_t>(logtype) >= 0) && (static_cast<uint32_t>(logtype) < std::size(_log_types_)) ? _log_types_[static_cast<uint32_t>(logtype)] : "unknown";
		md.username = userName;


		
		myinstance->LogToFile(md);
	};
};
