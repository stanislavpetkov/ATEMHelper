#pragma once
#include "String.h"
namespace pbn
{
	namespace OS
	{
		 pbn::String GetSystemDrive();
		 pbn::String GetSystemDir();
		 pbn::String GetSystemDirx86();

		 pbn::String GetProgramDataFolder();
		 pbn::String GetCommonFiles();
		 pbn::String GetCommonFiles32();
		 pbn::String get_logs_folder();
		 pbn::String GetAppConfigPath();
		

		 pbn::String GetWindowsProductName();
		 pbn::String GetWindowsReleaseId();
		 pbn::String GetWindowsCurrentBuild();
		 pbn::String GetWindowsVersionAsString();

		 //return ComputerNamePhysicalDnsFullyQualified
		 pbn::String get_computer_name();

		 pbn::String GetCurrentModuleFileName();
		 void* GetCurrentModuleInstance();
		 pbn::String get_current_module_path();


		 bool HasGlobalNamePrivilege();

		 bool GetSystemVolumeSerial(pbn::String& Serial);
		 bool GetPhysicalDriveSerialNumber(const uint32_t nDriveNumber, std::string& SerialNumber);
		 bool GetPhysicalDriveSerialNumber(const std::string& driveLetter, std::string& SerialNumber);
	};


}