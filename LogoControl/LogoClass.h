#pragma once
#include <string>
#include <memory>
#include <vector>
#include "LogoSel.h"

class LogoClassImpl;

class LogoClass
{
	
private:
	std::shared_ptr<LogoClassImpl> impl_;
public:
	LogoClass() = delete;
	LogoClass(const LogoClass&) = delete;
	LogoClass(LogoClass&&) = delete;
	LogoClass(const std::string& ip);
	~LogoClass();



	std::string Id() const;
	std::string Title() const;
	std::string Ip() const;
	std::string Model() const;
	
	std::string GetInputAStatus() const;
	std::string GetCPUUsage() const;
	std::string GetRAMUsage() const;
	std::string GetCardTemperatureFront() const;
	std::string GetCardTemperatureRear() const;
	std::string GetCardTime() const;
	std::string GetReferenceStatus() const;
	
	std::vector<bool> GetLogosStatus(const uint32_t pathNo);

	void Ip(const std::string& ipaddr);

	bool SetLogo(const uint32_t pathNo, const LogoSel sel);
	bool SetLogos(const LogoSel sel1, const LogoSel sel2);
};

