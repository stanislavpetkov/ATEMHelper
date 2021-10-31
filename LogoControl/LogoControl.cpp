// LogoControl.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include "LogoClass.h"
#include "ControllerClass.h"
#include <thread>
#include <WinSock2.h>
#include "../Common/ServiceBase.h"
#include "../Common/StringUtils.h"
#include <http.h>

constexpr auto service_name = L"PLANETA::LogoControl";
constexpr auto service_description = L"PLANETA::LogoControl";


bool PreinstallStart(const std::wstring& serviceUserName)
{
	HTTP_SERVICE_CONFIG_URLACL_SET info{};
	std::wstring httpServerUrl = L"http://*:9012/";

	PolicyClass policy(L"");
	std::wstring accountSidStr;
	if (!policy.getAccountStringSid(serviceUserName, accountSidStr)) return false;

	// Build a security descriptor in the required format


	auto securityDescriptorW = utf8_to_wstring(fmt::format("D:(A;;GX;;;{})", wstring_to_utf8(accountSidStr)));
	info.KeyDesc.pUrlPrefix = (PWSTR)httpServerUrl.c_str();
	info.ParamDesc.pStringSecurityDescriptor = (LPWSTR)securityDescriptorW.c_str();

	if (const auto ulResult = HttpInitialize(
		HTTPAPI_VERSION_2,
		HTTP_INITIALIZE_CONFIG,
		NULL); ulResult != NO_ERROR)
	{
		return false;
	}

	{
		bool bRes = false;
		while (true)
		{
			if (const auto res = HttpSetServiceConfiguration(0, HttpServiceConfigUrlAclInfo, reinterpret_cast<PVOID>(&info), sizeof(info), nullptr);
				res != NO_ERROR)
			{
				if (res != ERROR_ALREADY_EXISTS)
				{
					Log::error(__func__, "HttpSetServiceConfiguration failed 0x{:x}", static_cast<uint32_t>(res));
					bRes = false;
					break;
				}

				if (const auto delRes = HttpDeleteServiceConfiguration(0, HttpServiceConfigUrlAclInfo, reinterpret_cast<PVOID>(&info), sizeof(info), nullptr);
					delRes != NO_ERROR)
				{
					bRes = false;
					break;
				}
				else
				{
					continue;
				}
			}
			bRes = true;
			break;
		}
		HttpTerminate(HTTP_INITIALIZE_CONFIG, nullptr);

		if (!bRes) return false;
	}


	

	return true;
}


DWORD main_function(HANDLE quitHandle, bool bConsole)
{


	auto res = CoInitialize(nullptr);
	if (!((S_OK == res) || (S_FALSE == res))) {
		Log::error("main", "Error:: Can not call CoInitialize");
		return 1;
	}

	Log::SetConsoleLogging(bConsole);

#ifdef _DEBUG
	Log::SetLogLevel(LOGLEVEL::Debug);
#else
	Log::SetLogLevel(LOGLEVEL::Info);
#endif


	ControllerClass ctrlr;
	while (WAIT_TIMEOUT == WaitForSingleObject(quitHandle, INFINITE))
	{

	}
	return 0;

}
int main()
{



	
	INT rc;
	WSADATA wsaData;

	rc = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (rc) {
		throw std::exception("");
	}

	if (!TConsoleService::HasGlobalNamePrivilege())
	{
		Log::LogEventToFileSync(LOGLEVEL::Error, LOGTYPE::Audit, __func__, "Process Can NOT create Global Objects... Wrong user rights");
		return ERROR_ACCESS_DENIED;
	}


	TConsoleService::SetLogging();
	TConsoleService::init(service_name, service_description, L"", {}, PreinstallStart, main_function);
	return TConsoleService::start();
}
