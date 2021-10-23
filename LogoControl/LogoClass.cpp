#include "LogoClass.h"
#include "LogoClassImpl.h"


LogoClass::LogoClass(const std::string& ip) :
	impl_(std::make_shared<LogoClassImpl>(ip))
{
}

LogoClass::~LogoClass()
{
	impl_ = nullptr;
}

std::string LogoClass::Id() const
{
	return impl_->Id();
}

std::string LogoClass::Title() const
{
	return impl_->Title();
}

std::string LogoClass::Ip() const
{
	return impl_->Ip();
}

std::string LogoClass::Model() const
{
	return impl_->Model();
}


std::vector<bool> LogoClass::GetLogosStatus(const uint32_t pathNo)
{
	return impl_->GetLogosStatus(pathNo);
}

void LogoClass::Ip(const std::string& ipaddr)
{
	return impl_->Ip(ipaddr);
}

bool LogoClass::SetLogo(const uint32_t pathNo, const LogoSel sel)
{


	return impl_->SetLogo(pathNo, sel);
}

bool LogoClass::SetLogos(const LogoSel sel1, const LogoSel sel2)
{
	return impl_->SetLogos(sel1,sel2);
}


std::string LogoClass::GetInputAStatus() const
{
	return impl_->GetInputAStatus();
}


std::string LogoClass::GetCPUUsage() const
{
	return impl_->GetCPUUsage();
}

std::string LogoClass::GetRAMUsage() const
{
	return impl_->GetRAMUsage();
}

std::string LogoClass::GetCardTemperatureFront() const
{
	return impl_->GetCardTemperatureFront();
}

std::string LogoClass::GetCardTemperatureRear() const
{
	return impl_->GetCardTemperatureRear();
}

std::string LogoClass::GetCardTime() const
{
	return impl_->GetCardTime();
}

std::string LogoClass::GetReferenceStatus() const
{
	return impl_->GetReferenceStatus();
}