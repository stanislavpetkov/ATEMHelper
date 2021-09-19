#include <Windows.h>
#include <winioctl.h>

#include "OSutils.h"
#include "../StringUtils.h"

#include <ShlObj.h>
#include <Knownfolders.h>
#include "fs.h"
#include "../../LibLogging/LibLogging.h"

namespace pbn
{
	inline bool getKnownFolder(const KNOWNFOLDERID& known_folder, std::wstring& folder)
	{
		PWSTR ppszPath = nullptr;
		try
		{
			if (E_FAIL == SHGetKnownFolderPath(known_folder, 0, nullptr, &ppszPath))
			{
				return false;
			}
			if (ppszPath)
			{
				folder = std::wstring(ppszPath);
				CoTaskMemFree(ppszPath);
				return true;
			}
		}
		catch (...)
		{

		}
		return false;
	}
	pbn::String OS::GetProgramDataFolder()
	{
		std::wstring folderPath;
		if (!getKnownFolder(FOLDERID_ProgramData, folderPath)) return {};
		return folderPath;
	}

	pbn::String OS::GetCommonFiles32()
	{
		std::wstring folderPath;
		if (!getKnownFolder(FOLDERID_ProgramFilesCommonX86, folderPath)) return {};
		return folderPath;
	}

	pbn::String OS::GetCommonFiles()
	{
		std::wstring folderPath;
		if (!getKnownFolder(FOLDERID_ProgramFilesCommon, folderPath)) return {};
		return folderPath;
	}

	pbn::String OS::GetSystemDir()
	{
		std::wstring folderPath;
		if (!getKnownFolder(FOLDERID_System, folderPath)) return {};
		return folderPath;
	}
	pbn::String OS::GetSystemDirx86()
	{
		std::wstring folderPath;
		if (!getKnownFolder(FOLDERID_SystemX86, folderPath)) return {};
		return folderPath;
	}


	pbn::String OS::GetSystemDrive()
	{
		auto sysdrv = GetSystemDir();
		pbn::fs::Path p(sysdrv);
		return p.get_root();		
	}


	inline std::wstring GetVersionkeysFromRegistry(const std::wstring& key)
	{
		constexpr auto reg_path = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";

		DWORD sz = 0;
		if (ERROR_SUCCESS != RegGetValueW(HKEY_LOCAL_MACHINE, reg_path, key.c_str(), RRF_RT_REG_SZ, nullptr, nullptr, &sz))
		{
			return L"<UNKNOWN>";
		}
		std::wstring value;
		value.resize(sz / sizeof(wchar_t));
		sz = (DWORD)(value.size() * sizeof(wchar_t));
		if (ERROR_SUCCESS != RegGetValueW(HKEY_LOCAL_MACHINE, reg_path, key.c_str(), RRF_RT_REG_SZ, nullptr, (LPWSTR)value.c_str(), &sz))
		{
			return L"<UNKNOWN>";
		}
		value.resize((sz / sizeof(wchar_t)) - 1);
		return value;
	}


	pbn::String OS::GetWindowsProductName()
	{
		return GetVersionkeysFromRegistry(L"ProductName");
	}

	pbn::String OS::GetWindowsReleaseId()
	{
		return GetVersionkeysFromRegistry(L"ReleaseId");
	}

	pbn::String OS::GetWindowsCurrentBuild()
	{
		return GetVersionkeysFromRegistry(L"CurrentBuild");
	}

	pbn::String OS::GetWindowsVersionAsString()
	{
		std::wstring product_name{ GetVersionkeysFromRegistry(L"ProductName") };
		std::wstring release_id{ GetVersionkeysFromRegistry(L"ReleaseId") };
		std::wstring build_id{ GetVersionkeysFromRegistry(L"CurrentBuild") };

		return product_name + L", Release: " + release_id + L", Build: " + build_id;
	}


	
	pbn::String OS::get_computer_name()
	{
		DWORD sz = 0;
		BOOL res = GetComputerNameExW(
			ComputerNamePhysicalDnsFullyQualified,
			nullptr,
			&sz
		);

		std::wstring tres;
		tres.resize(sz);

		res = GetComputerNameExW(
			ComputerNamePhysicalDnsFullyQualified,
			(LPWSTR)tres.c_str(),
			&sz
		);
		tres.resize(sz);
		return tres;
	}


	pbn::String OS::get_logs_folder()
	{
		return  GetProgramDataFolder();
	}

	pbn::String OS::GetAppConfigPath()
	{
		std::wstring programData = GetProgramDataFolder();
		if (programData.empty()) return L"C:\\!!!AppConfigFolder!!!\\";
		std::wstring fn = fs::get_file_name_without_extension(GetCurrentModuleFileName());

		fs::Path p(programData);

		p.append(L"PlayBoxNeo");
		p.append(fn);
		return p.wstring();
	}


	//use as a pointer in my module
	static const void MyFunc()
	{};

	pbn::String OS::GetCurrentModuleFileName()
	{
		HMODULE handle;
		if (FALSE == GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (PWSTR)&MyFunc, &handle))
		{
			return L"Unknown";
		}
		std::wstring fn; fn.resize(1024);
		auto res = GetModuleFileNameW(handle, const_cast<LPWSTR>(fn.c_str()), static_cast<DWORD>(fn.size()));
		auto gle = GetLastError();
		if (gle != 0)  return L"UnkSZ";
		fn.resize(res);
		pbn::fs::Path p(fn);
		return p.file_name();
	}

	pbn::String OS::get_current_module_path()
	{
		HMODULE handle;
		if (FALSE == GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (PWSTR)&MyFunc, &handle))
		{
			return L"Unknown";
		}
		std::wstring fn; fn.resize(1024);
		auto res = GetModuleFileNameW(handle, const_cast<LPWSTR>(fn.c_str()), static_cast<DWORD>(fn.size()));
		auto gle = GetLastError();
		if (gle != 0)  return L"UnkSZ";
		fn.resize(res);
		pbn::fs::Path p(fn);
		return p.parent_path();
	}


	bool OS::HasGlobalNamePrivilege()
	{
		LUID luid;

		if (!LookupPrivilegeValue(
			NULL,            // lookup privilege on local system
			SE_CREATE_GLOBAL_NAME,   // privilege to lookup 
			&luid))        // receives LUID of privilege
		{
			printf("LookupPrivilegeValue error: %u\n", GetLastError());
			return false;
		}

		HANDLE hToken = nullptr;
		bool bRes = false;
		if (FALSE == OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
		{
			if (hToken != nullptr)
			{
				CloseHandle(hToken);
			}
			return false;
		}






		DWORD cbSize = 0;

		//Get The buffer size in cbSize
		if (TRUE != GetTokenInformation(hToken, TokenPrivileges, nullptr, 0, &cbSize))
		{
			if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
				if (hToken != nullptr)
				{
					CloseHandle(hToken);
				}
				return false;
			}
		}

		std::vector<uint8_t> vecB(cbSize);
		PTOKEN_PRIVILEGES privileges = (PTOKEN_PRIVILEGES)vecB.data();


		if (TRUE != GetTokenInformation(hToken, TokenPrivileges, privileges, (DWORD)vecB.size(), &cbSize))
		{

			printf("GetTokenInformation error: %u\n", GetLastError());
			if (hToken != nullptr)
			{
				CloseHandle(hToken);
			}
			return false;
		}


		for (size_t i = 0; i < privileges->PrivilegeCount; i++)
		{
			auto& priv = privileges->Privileges[i];
			if ((luid.HighPart == priv.Luid.HighPart) &&
				(luid.LowPart == priv.Luid.LowPart))
			{
				bRes = (priv.Attributes & SE_PRIVILEGE_ENABLED) == SE_PRIVILEGE_ENABLED;// || (priv.Attributes & SE_PRIVILEGE_ENABLED_BY_DEFAULT));
				break;
			}
		}




		if (hToken != nullptr)
		{
			CloseHandle(hToken);
		}
		return bRes;
	}



	bool OS::GetSystemVolumeSerial(pbn::String& Serial)
	{
		auto sysDrive = GetSystemDrive();
		

		WCHAR volumeName[MAX_PATH + 1] = { 0 };
		WCHAR fileSystemName[MAX_PATH + 1] = { 0 };
		DWORD serialNumber = 0;
		DWORD maxComponentLen = 0;
		DWORD fileSystemFlags = 0;
		if (GetVolumeInformationW(
			sysDrive.c_str(),
			volumeName,
			_countof(volumeName),
			&serialNumber,
			&maxComponentLen,
			&fileSystemFlags,
			fileSystemName,
			_countof(fileSystemName)) == FALSE) return false;
		Serial = std::to_string(serialNumber);
		return true;
	}


	bool OS::GetPhysicalDriveSerialNumber(const std::string & driveLetter /*C: for example*/, std::string & SerialNumber)
	{

		SerialNumber = "";

		// Format physical drive path (may be '\\.\PhysicalDrive0', '\\.\PhysicalDrive1' and so on).
		pbn::String drive_path = L"\\\\.\\";
		drive_path.append( driveLetter);


		// call CreateFile to get a handle to physical drive
		HANDLE hDevice = ::CreateFileW(drive_path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL, OPEN_EXISTING, 0, NULL);

		if (INVALID_HANDLE_VALUE == hDevice)
		{
			Log::error(__FUNCTION__, "Can not access the drive. Err {}", ::GetLastError());
			return false;
		}

		// set the input STORAGE_PROPERTY_QUERY data structure
		STORAGE_PROPERTY_QUERY storagePropertyQuery{};

		storagePropertyQuery.PropertyId = StorageDeviceProperty;
		storagePropertyQuery.QueryType = PropertyStandardQuery;

		// get the necessary output buffer size
		STORAGE_DESCRIPTOR_HEADER storageDescriptorHeader = { 0 };
		DWORD dwBytesReturned = 0;
		if (!::DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY,
			&storagePropertyQuery, sizeof(STORAGE_PROPERTY_QUERY),
			&storageDescriptorHeader, sizeof(STORAGE_DESCRIPTOR_HEADER),
			&dwBytesReturned, NULL))
		{
			Log::error(__FUNCTION__, "DeviceIoControl failed. Err {}", ::GetLastError());
			::CloseHandle(hDevice);
			return false;
		}

		// allocate the necessary memory for the output buffer
		const DWORD dwOutBufferSize = storageDescriptorHeader.Size;
		std::vector<char> OutBuffer(dwOutBufferSize);



		// get the storage device descriptor
		if (!::DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY,
			&storagePropertyQuery, sizeof(STORAGE_PROPERTY_QUERY),
			OutBuffer.data(), dwOutBufferSize,
			&dwBytesReturned, NULL))
		{
			Log::error(__FUNCTION__, "DeviceIoControl failed 2. Err {}", ::GetLastError());
			::CloseHandle(hDevice);
			return false;
		}

		// Now, the output buffer points to a STORAGE_DEVICE_DESCRIPTOR structure
		// followed by additional info like vendor ID, product ID, serial number, and so on.
		STORAGE_DEVICE_DESCRIPTOR* pDeviceDescriptor = (STORAGE_DEVICE_DESCRIPTOR*)OutBuffer.data();
		const DWORD dwSerialNumberOffset = pDeviceDescriptor->SerialNumberOffset;
		if (dwSerialNumberOffset != 0)
		{
			SerialNumber = std::string(reinterpret_cast<char*>(OutBuffer.data() + dwSerialNumberOffset));
			// finally, get the serial number			
		}

		// perform cleanup and return
		::CloseHandle(hDevice);
		return true;
	}

	bool OS::GetPhysicalDriveSerialNumber(const uint32_t nDriveNumber, std::string& SerialNumber)
	{

		SerialNumber = "";

		// Format physical drive path (may be '\\.\PhysicalDrive0', '\\.\PhysicalDrive1' and so on).
		auto drive_path = L"\\\\.\\PhysicalDrive" + std::to_wstring(nDriveNumber);


		// call CreateFile to get a handle to physical drive
		HANDLE hDevice = ::CreateFileW(drive_path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL, OPEN_EXISTING, 0, NULL);

		if (INVALID_HANDLE_VALUE == hDevice)
		{
			Log::error(__FUNCTION__, "Can not access the drive. Err {}", ::GetLastError());
			return false;
		}

		// set the input STORAGE_PROPERTY_QUERY data structure
		STORAGE_PROPERTY_QUERY storagePropertyQuery{};

		storagePropertyQuery.PropertyId = StorageDeviceProperty;
		storagePropertyQuery.QueryType = PropertyStandardQuery;

		// get the necessary output buffer size
		STORAGE_DESCRIPTOR_HEADER storageDescriptorHeader = { 0 };
		DWORD dwBytesReturned = 0;
		if (!::DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY,
			&storagePropertyQuery, sizeof(STORAGE_PROPERTY_QUERY),
			&storageDescriptorHeader, sizeof(STORAGE_DESCRIPTOR_HEADER),
			&dwBytesReturned, NULL))
		{
			Log::error(__FUNCTION__, "DeviceIoControl failed. Err {}", ::GetLastError());
			::CloseHandle(hDevice);
			return false;
		}

		// allocate the necessary memory for the output buffer
		const DWORD dwOutBufferSize = storageDescriptorHeader.Size;
		std::vector<char> OutBuffer(dwOutBufferSize);



		// get the storage device descriptor
		if (!::DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY,
			&storagePropertyQuery, sizeof(STORAGE_PROPERTY_QUERY),
			OutBuffer.data(), dwOutBufferSize,
			&dwBytesReturned, NULL))
		{
			Log::error(__FUNCTION__, "DeviceIoControl failed 2. Err {}", ::GetLastError());
			::CloseHandle(hDevice);
			return false;
		}

		// Now, the output buffer points to a STORAGE_DEVICE_DESCRIPTOR structure
		// followed by additional info like vendor ID, product ID, serial number, and so on.
		STORAGE_DEVICE_DESCRIPTOR* pDeviceDescriptor = (STORAGE_DEVICE_DESCRIPTOR*)OutBuffer.data();
		const DWORD dwSerialNumberOffset = pDeviceDescriptor->SerialNumberOffset;
		if (dwSerialNumberOffset != 0)
		{
			SerialNumber = std::string(reinterpret_cast<char*>(OutBuffer.data() + dwSerialNumberOffset));
			// finally, get the serial number			
		}

		// perform cleanup and return
		::CloseHandle(hDevice);
		return true;
	}

}