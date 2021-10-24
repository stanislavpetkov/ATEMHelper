#pragma once
#include <cstdint>
#include <string>
#include <atomic>
#include <vector>
#include <thread>
#include <concurrent_queue.h>
#include "../Common/atomic_lock.h"
#include "LogoCmdDefs.h"
#include "LogoSel.h"
#include "../Common/Clock/Clock.h"
#include "../LibLogging/LibLogging.h"


struct cmdPack;

struct LogoPath
{
	std::string id;
	std::vector<bool> data;
};

class LogoClass
{
private:
	concurrency::concurrent_queue<std::shared_ptr<cmdPack>> tx_queue;
	std::atomic<bool> bStop;

	std::atomic<bool> data_lock_ = false;
	int64_t last_update_time=0;
	std::string title_;
	std::string cardUUID = "";
	std::string ip_;
	std::string productModel_;
	std::thread comms;

	LogoPath path1;
	LogoPath path2;

	

	std::string cardTimeId;
	std::string cardTime;

	std::string referenceId;
	std::string referenceStat;

	std::string cpuUsageId;
	std::string cpuUsage;
	
	std::string cardTempFrontId;
	std::string cardTempFront;
	
	std::string cardTempRearId;
	std::string cardTempRear;

	std::string ramUsageId;
	std::string ramUsage;

	std::string inputAId;
	std::string inputAStat;

	bool handleDeviceInfo(nlohmann::json& data);
	
	void handleLogoPathChanged(const size_t logoPathNo, const std::vector<int>& value);
	void commsFn();

	template<typename DT>
	void UpdateData(const DT& newData, DT& prop)
	{
		atomic_lock lock(data_lock_);
		if (newData == prop) return;
		prop = newData;
		last_update_time = ClockHighRes::get_time();

		Log::info(__func__, "Something updated");
	}

	template<typename DT>
	void UpdateDataNOLT(const DT& newData, DT& prop)
	{
		atomic_lock lock(data_lock_);
		if (newData == prop) return;
		prop = newData;
	}


	template<>
	void UpdateData<LogoPath>(const LogoPath& newData, LogoPath& prop)
	{
		atomic_lock lock(data_lock_);
		if ((newData.id == prop.id) && 
			(newData.data == prop.data))
			return;
		prop = newData;
		last_update_time = ClockHighRes::get_time();

		Log::info(__func__, "LogoPath Update");
	}

	template<>
	void UpdateData<std::vector<bool>>(const std::vector<bool>& newData, std::vector<bool>& prop)
	{
		atomic_lock lock(data_lock_);
		if (newData == prop) return;
		prop = newData;
		last_update_time = ClockHighRes::get_time();
		Log::info(__func__, "LogoPath Data Update");
	}

public:
	std::string GetInputAStatus();	
	std::string GetCPUUsage();
	std::string GetRAMUsage();
	std::string GetCardTemperatureFront();
	std::string GetCardTemperatureRear();
	std::string GetCardTime();	
	std::string GetReferenceStatus();	
	int64_t GetLastUpdateTime();
	bool SetLogo(const uint32_t pathNo, const LogoSel sel);
	bool SetLogos(const LogoSel selPath1, const LogoSel selPath2);
	std::string Id();	
	std::string Model();	
	std::string Title();	
	std::string Ip();	
	void Ip(const std::string& ipaddr);
	
	std::vector<bool> GetLogosStatus(const uint32_t pathNo);
	

	
	
	LogoClass(const std::string& ip);
	~LogoClass();
};



