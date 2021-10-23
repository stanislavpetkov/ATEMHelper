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

struct cmdPack;
class LogoClassImpl
{
private:
	concurrency::concurrent_queue<std::shared_ptr<cmdPack>> tx_queue;
	std::atomic<bool> bStop;

	std::atomic<bool> data_lock = false;
	std::string title_;
	std::string cardUUID = "";
	std::string ip_;
	std::string productModel_;
	std::thread comms;
	std::vector<bool> logoPath1Data;
	std::vector<bool> logoPath2Data;
	std::string logoPath1InserterId;
	std::string logoPath2InserterId;

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

public:
	std::string GetInputAStatus();	
	std::string GetCPUUsage();
	std::string GetRAMUsage();
	std::string GetCardTemperatureFront();
	std::string GetCardTemperatureRear();
	std::string GetCardTime();	
	std::string GetReferenceStatus();	
	bool SetLogo(const uint32_t pathNo, const LogoSel sel);
	bool SetLogos(const LogoSel selPath1, const LogoSel selPath2);
	std::string Id();	
	std::string Model();	
	std::string Title();	
	std::string Ip();	
	void Ip(const std::string& ipaddr);
	
	std::vector<bool> GetLogosStatus(const uint32_t pathNo);
	

	
	
	LogoClassImpl(const std::string& ip);
	~LogoClassImpl();
};



