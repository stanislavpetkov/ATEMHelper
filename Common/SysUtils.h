#pragma once
#include <string>
#include <stdint.h>
#include <intrin.h>

#include <filesystem>

#if (defined(WINDOWS) || defined(_WIN32))
#include <ShlObj.h>
#include <Knownfolders.h>
#include "StringUtils.h"
#else
#include <unistd.h>
#include <limits.h>
#endif

#include "../LibLogging/LibLogging.h"

#include <winioctl.h>
#ifdef _WIN32

//  Windows
#define cpuid(info, x)    __cpuidex(info, x, 0)

#else

//  GCC Intrinsics
#include <cpuid.h>
void cpuid(int info[4], int InfoType) {
	__cpuid_count(InfoType, 0, info[0], info[1], info[2], info[3]);
}

#endif

namespace SysUtils
{


	inline uint32_t CountSetBits(size_t bitMask)
	{
		uint32_t LSHIFT = sizeof(size_t) * 8 - 1;
		uint32_t bitSetCount = 0;
		size_t bitTest = (size_t)1 << LSHIFT;
		uint32_t i;

		for (i = 0; i <= LSHIFT; ++i)
		{
			bitSetCount += ((bitMask & bitTest) ? 1 : 0);
			bitTest /= 2;
		}

		return bitSetCount;
	}

	struct CPUExtInfo
	{
		uint32_t numaNodesCount;
		uint32_t processorPackageCount;
		uint32_t processorCoreCount;
		uint32_t logicalProcessorCount;
		size_t processorL1CacheCount;
		size_t processorL2CacheCount;
		size_t processorL3CacheCount;
	};

	static bool GetProcessorsExtendedInfo(CPUExtInfo& info)
	{
		info = {};
		BOOL done = FALSE;
		PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer = NULL;
		PSYSTEM_LOGICAL_PROCESSOR_INFORMATION ptr = NULL;
		DWORD returnLength = 0;
		DWORD logicalProcessorCount = 0;
		DWORD numaNodeCount = 0;
		DWORD processorCoreCount = 0;
		DWORD processorL1CacheCount = 0;
		DWORD processorL2CacheCount = 0;
		DWORD processorL3CacheCount = 0;
		DWORD processorPackageCount = 0;
		DWORD byteOffset = 0;
		PCACHE_DESCRIPTOR Cache;

		while (!done)
		{
			DWORD rc = GetLogicalProcessorInformation(buffer, &returnLength);

			if (FALSE == rc)
			{
				if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
				{
					if (buffer)
						free(buffer);

					buffer = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)malloc(
						returnLength);

					if (NULL == buffer)
					{
						//Log::error(__func__, "Error: Allocation failure");
						return false;
					}
				}
				else
				{
					//Log::error(__func__, "Error: {}", GetLastError());
					return false;
				}
			}
			else
			{
				done = TRUE;
			}
		}

		ptr = buffer;

		if (ptr != nullptr)
		{
			while (byteOffset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) <= returnLength)
			{
				switch (ptr->Relationship)
				{
				case RelationNumaNode:
					// Non-NUMA systems report a single record of this type.
					numaNodeCount++;
					break;

				case RelationProcessorCore:
					processorCoreCount++;

					// A hyperthreaded core supplies more than one logical processor.
					logicalProcessorCount += CountSetBits(ptr->ProcessorMask);
					break;

				case RelationCache:
					// Cache data is in ptr->Cache, one CACHE_DESCRIPTOR structure for each cache.
					Cache = &ptr->Cache;
					if (Cache->Level == 1)
					{
						processorL1CacheCount++;
					}
					else if (Cache->Level == 2)
					{
						processorL2CacheCount++;
					}
					else if (Cache->Level == 3)
					{
						processorL3CacheCount++;
					}
					break;

				case RelationProcessorPackage:
					// Logical processors share a physical package.
					processorPackageCount++;
					break;

				default:
					//Log::error(__func__,"Error: Unsupported LOGICAL_PROCESSOR_RELATIONSHIP value.");
					break;
				}
				byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
				ptr++;
			}
		}

		info.numaNodesCount = numaNodeCount;
		info.processorPackageCount = processorPackageCount;
		info.processorCoreCount = processorCoreCount;
		info.logicalProcessorCount = logicalProcessorCount;

		info.processorL1CacheCount = processorL1CacheCount;
		info.processorL2CacheCount = processorL2CacheCount;
		info.processorL3CacheCount = processorL3CacheCount;

		free(buffer);

		return true;
	}


#pragma warning(disable:4505) 
	static std::string GetVersionkeysFromRegistry(const std::wstring& key)
	{
		constexpr auto reg_path = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";

		DWORD sz = 0;
		if (ERROR_SUCCESS != RegGetValueW(HKEY_LOCAL_MACHINE, reg_path, key.c_str(), RRF_RT_REG_SZ, nullptr, nullptr, &sz))
		{
			return "<UNKNOWN>";
		}
		std::wstring value;
		value.resize(sz / sizeof(wchar_t));
		sz = (DWORD)(value.size() * sizeof(wchar_t));
		if (ERROR_SUCCESS != RegGetValueW(HKEY_LOCAL_MACHINE, reg_path, key.c_str(), RRF_RT_REG_SZ, nullptr, (LPWSTR)value.c_str(), &sz))
		{
			return "<UNKNOWN>";
		}
		value.resize((sz / sizeof(wchar_t)) - 1);
		return wstring_to_utf8(value);
	}


	static std::string GetWindowsVersionAsString()
	{
		std::string product_name{ GetVersionkeysFromRegistry(L"ProductName") };
		std::string release_id{ GetVersionkeysFromRegistry(L"ReleaseId") };
		std::string build_id{ GetVersionkeysFromRegistry(L"CurrentBuild") };

		return product_name + ", Release: " + release_id + ", Build: " + build_id;
	}

	static std::string GetCPUString()
	{
		// Get extended ids.
		int CPUInfo[4] = { -1 };
		__cpuid(CPUInfo, 0x80000000);
		unsigned int nExIds = CPUInfo[0];

		// Get the information associated with each extended ID.
		char CPUBrandString[0x40] = { 0 };
		for (unsigned int i = 0x80000000; i <= nExIds; ++i)
		{
			__cpuid(CPUInfo, i);

			// Interpret CPU brand string and cache information.
			if (i == 0x80000002)
			{
				memcpy(CPUBrandString,
					CPUInfo,
					sizeof(CPUInfo));
			}
			else if (i == 0x80000003)
			{
				memcpy(CPUBrandString + 16,
					CPUInfo,
					sizeof(CPUInfo));
			}
			else if (i == 0x80000004)
			{
				memcpy(CPUBrandString + 32, CPUInfo, sizeof(CPUInfo));
			}
		}

		std::string cpustr(CPUBrandString);
		return cpustr;
	}

	static void isCPUOK()
	{


		//  Misc.
		bool HW_MMX = false;
		bool HW_x64 = false;
		bool HW_ABM = false;      // Advanced Bit Manipulation
		bool HW_RDRAND = false;
		bool HW_BMI1 = false;
		bool HW_BMI2 = false;
		bool HW_ADX = false;
		bool HW_PREFETCHWT1 = false;

		//  SIMD: 128-bit
		bool HW_SSE = false;
		bool HW_SSE2 = false;
		bool HW_SSE3 = false;
		bool HW_SSSE3 = false;
		bool HW_SSE41 = false;
		bool HW_SSE42 = false;
		bool HW_SSE4a = false;
		bool HW_AES = false;
		bool HW_SHA = false;

		//  SIMD: 256-bit
		bool HW_AVX = false;
		bool HW_XOP = false;
		bool HW_FMA3 = false;
		bool HW_FMA4 = false;
		bool HW_AVX2 = false;

		//  SIMD: 512-bit
		bool HW_AVX512F = false;    //  AVX512 Foundation
		bool HW_AVX512CD = false;   //  AVX512 Conflict Detection
		bool HW_AVX512PF = false;   //  AVX512 Prefetch
		bool HW_AVX512ER = false;   //  AVX512 Exponential + Reciprocal
		bool HW_AVX512VL = false;   //  AVX512 Vector Length Extensions
		bool HW_AVX512BW = false;   //  AVX512 Byte + Word
		bool HW_AVX512DQ = false;   //  AVX512 Doubleword + Quadword
		bool HW_AVX512IFMA = false; //  AVX512 Integer 52-bit Fused Multiply-Add
		bool HW_AVX512VBMI = false; //  AVX512 Vector Byte Manipulation Instructions

		int info[4];
		cpuid(info, 0);
		int nIds = info[0];

		cpuid(info, 0x80000000);
		unsigned nExIds = info[0];

		//  Detect Features
		if (nIds >= 0x00000001) {
			cpuid(info, 0x00000001);
			HW_MMX = (info[3] & ((int)1 << 23)) != 0;
			HW_SSE = (info[3] & ((int)1 << 25)) != 0;
			HW_SSE2 = (info[3] & ((int)1 << 26)) != 0;
			HW_SSE3 = (info[2] & ((int)1 << 0)) != 0;

			HW_SSSE3 = (info[2] & ((int)1 << 9)) != 0;
			HW_SSE41 = (info[2] & ((int)1 << 19)) != 0;
			HW_SSE42 = (info[2] & ((int)1 << 20)) != 0;
			HW_AES = (info[2] & ((int)1 << 25)) != 0;

			HW_AVX = (info[2] & ((int)1 << 28)) != 0;
			HW_FMA3 = (info[2] & ((int)1 << 12)) != 0;

			HW_RDRAND = (info[2] & ((int)1 << 30)) != 0;
		}
		if (nIds >= 0x00000007) {
			cpuid(info, 0x00000007);
			HW_AVX2 = (info[1] & ((int)1 << 5)) != 0;

			HW_BMI1 = (info[1] & ((int)1 << 3)) != 0;
			HW_BMI2 = (info[1] & ((int)1 << 8)) != 0;
			HW_ADX = (info[1] & ((int)1 << 19)) != 0;
			HW_SHA = (info[1] & ((int)1 << 29)) != 0;
			HW_PREFETCHWT1 = (info[2] & ((int)1 << 0)) != 0;

			HW_AVX512F = (info[1] & ((int)1 << 16)) != 0;
			HW_AVX512CD = (info[1] & ((int)1 << 28)) != 0;
			HW_AVX512PF = (info[1] & ((int)1 << 26)) != 0;
			HW_AVX512ER = (info[1] & ((int)1 << 27)) != 0;
			HW_AVX512VL = (info[1] & ((int)1 << 31)) != 0;
			HW_AVX512BW = (info[1] & ((int)1 << 30)) != 0;
			HW_AVX512DQ = (info[1] & ((int)1 << 17)) != 0;
			HW_AVX512IFMA = (info[1] & ((int)1 << 21)) != 0;
			HW_AVX512VBMI = (info[2] & ((int)1 << 1)) != 0;
		}
		if (nExIds >= 0x80000001) {
			cpuid(info, 0x80000001);
			HW_x64 = (info[3] & ((int)1 << 29)) != 0;
			HW_ABM = (info[2] & ((int)1 << 5)) != 0;
			HW_SSE4a = (info[2] & ((int)1 << 6)) != 0;
			HW_FMA4 = (info[2] & ((int)1 << 16)) != 0;
			HW_XOP = (info[2] & ((int)1 << 11)) != 0;
		}




#ifdef __AVX__
		if (!HW_AVX)
		{
			printf("Unsupported CPU platform. AVX Required but not supported.\n");
			TerminateProcess(GetCurrentProcess(), 1);
		}
#endif

#ifdef __AVX2__
		if (!HW_AVX2)
		{
			printf("Unsupported CPU platform. AVX2 Required but not supported.\n");
			TerminateProcess(GetCurrentProcess(), 1);
		}
#endif

	}

	namespace __internal__
	{

#if (defined(WINDOWS) || defined(_WIN32))
		static inline bool getenv(const KNOWNFOLDERID& known_folder, std::wstring& folder)
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
#else

#endif

	}




	static inline std::wstring GetFileNameWithoutExtension(const std::wstring& path)
	{
#ifdef _HAS_CXX17
		namespace fs = std::filesystem;

		fs::path f(path);
		std::wstring fn = f.filename().wstring();
		fn = fn.substr(0, fn.size() - f.extension().wstring().size());
		return fn;
#else
		static_assert(true, "We shouldn't be here");
		return L"_C++17 REUQIRED";
#endif
	}

	static inline  std::string GetFileNameWithoutExtension(const std::string& path)
	{

#ifdef _HAS_CXX17
		namespace fs = std::filesystem;
		fs::path f(path);
		std::string fn = f.filename().generic_u8string();
		fn = fn.substr(0, fn.size() - f.extension().generic_u8string().size());
		return fn;
#else
		static_assert(true, "We shouldn't be here");
		return "_C++17 REUQIRED";
#endif
	}


	static inline bool CreateDir(const std::string& cPath)
	{
#ifdef _HAS_CXX17
		namespace fs = std::filesystem;



		if (fs::exists(cPath))
			return true;

		size_t pos = 0;
		do
		{
			std::error_code errc;
			pos = cPath.find_first_of("\\/", pos + 1);
			fs::create_directory(cPath.substr(0, pos), errc);
			std::string ss = errc.message();
		} while (pos != std::string::npos);
		return true;
#else
		static_assert(true, "We shouldn't be here");
		return false;
#endif
	}



	static inline bool CreateDir(const std::wstring& cPath)
	{
#if (defined(WINDOWS) || defined(_WIN32))

		namespace fs = std::filesystem;

		if (fs::exists(cPath))
			return true;

		size_t pos = 0;
		do
		{
			std::error_code errc;
			pos = cPath.find_first_of(L"\\/", pos + 1);
			fs::create_directory(cPath.substr(0, pos), errc);
		} while (pos != std::string::npos);
		return true;
#else
		return false;
#endif
	}





#if (defined(WINDOWS) || defined(_WIN32))
	static bool GetProgramDataFolder(std::wstring& folderPath)
	{

		return __internal__::getenv(FOLDERID_ProgramData, folderPath);

	}


	static bool GetCommonFiles32(std::wstring& folderPath)
	{

		return __internal__::getenv(FOLDERID_ProgramFilesCommonX86, folderPath);

	}

	static bool GetSystemDrive(std::string& folderPath)
	{
		char* buf = nullptr;
		size_t sz = 0;
		if (_dupenv_s(&buf, &sz, "SystemDrive") == 0 && buf != nullptr)
		{
			if (buf)
			{
				folderPath.assign(buf);
				free(buf);
			}
		}

		return (folderPath.empty()) ? false : true;
	}

#endif

#if (defined(WINDOWS) || defined(_WIN32))
	static std::wstring get_logs_folder()
	{
		std::wstring f;
		GetProgramDataFolder(f);

		return f;
	}
#else
	static inline std::string get_logs_folder()
	{
		return "./log";
	}
#endif
	static inline std::string get_computer_name()
	{
#ifdef GetComputerNameEx

		DWORD sz = 0;
		BOOL res = GetComputerNameExA(
			ComputerNameDnsFullyQualified,
			nullptr,
			&sz
		);

		std::string tres;
		tres.resize(sz);

		res = GetComputerNameExA(
			ComputerNameDnsFullyQualified,
			(LPSTR)tres.c_str(),
			&sz
		);
		tres.resize(sz);
		return tres;
#else

		char hostname[HOST_NAME_MAX];
		memset(hostname, 0, sizeof(hostname));
		gethostname(hostname, HOST_NAME_MAX);

		std::string s(&hostname[0]);
		memset(hostname, 0, sizeof(hostname));

		if (getdomainname(hostname, HOST_NAME_MAX) == 0)
		{
			s.append(".");
			s.append(&hostname[0]);
		}
		else
		{
			s.append(".nodomain");
		}
		return s;
#endif
	}


#ifdef __linux__
	static inline std::string get_process_name(const pid_t pid) {
		std::string name_;

		name_.resize(BUFSIZ);
		char procfile[BUFSIZ];
		sprintf(procfile, "/proc/%d/cmdline", pid);
		FILE* f = fopen(procfile, "r");
		if (f) {
			size_t size;

			size = fread((void*)name_.data(), sizeof(char), sizeof(procfile), f);
			fclose(f);
			if (size > 0) {
				name_.resize(size);
				//name contains the whole cmdline divided by null
				auto r = name_.find('\0');
				if (r != std::string::npos)
					name_.resize(r);


				if ('\n' == name_[name_.size() - 1])
					name_.resize(name_.size() - 1);
				if ('\0' == name_[name_.size() - 1])
					name_.resize(name_.size() - 1);
			}
			else
			{
				return "UNK_ZERO";
			}

			return name_;
		}
		return "UNKNOWN";
	}
#endif

#if (!(defined(WINDOWS) || defined(_WIN32)))
#define HMODULE size_t
#endif

	static inline std::string ModuleName(HMODULE mdl)
	{
#ifdef GetModuleFileName
		wchar_t tmp[MAX_PATH];
		ZeroMemory(tmp, MAX_PATH * sizeof(wchar_t));

		DWORD actualLen = GetModuleFileNameW(mdl, &tmp[0], MAX_PATH);
		std::wstring modName(&tmp[0], actualLen);
		size_t x = modName.find_last_of('\\');
		if (x != std::wstring::npos)
		{
			modName = modName.substr(x + 1);
		}

		x = modName.find_last_of('/');
		if (x != std::wstring::npos)
		{
			modName = modName.substr(x + 1);
		}

		return  wstring_to_utf8(modName); //utf8aware modulename
#else
		pid_t pid = getpid();


		return GetFileNameWithoutExtension(get_process_name(pid));
#endif
	}

	static std::string GetCurrentModuleName()
	{
		HMODULE hModule = nullptr;
#ifdef GetModuleFileName

		GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
			GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			(LPCSTR)&GetCurrentModuleName, &hModule);
#endif
		return ModuleName(hModule);
	}



	

	inline bool GetPhysicalDriveSerialNumber(const std::string& driveLetter /*C: for example*/, std::string& SerialNumber)
	{

		SerialNumber = "";

		// Format physical drive path (may be '\\.\PhysicalDrive0', '\\.\PhysicalDrive1' and so on).
		auto drive_path = L"\\\\.\\" + utf8_to_wstring(driveLetter);


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

	inline bool GetPhysicalDriveSerialNumber(const uint32_t nDriveNumber, std::string& SerialNumber)
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

	static bool GetSystemVolumeSerial(std::string& Serial)
	{
		std::string sysDrive;
		if (!SysUtils::GetSystemDrive(sysDrive)) return false;

		WCHAR volumeName[MAX_PATH + 1] = { 0 };
		WCHAR fileSystemName[MAX_PATH + 1] = { 0 };
		DWORD serialNumber = 0;
		DWORD maxComponentLen = 0;
		DWORD fileSystemFlags = 0;
		if (GetVolumeInformationW(
			utf8_to_wstring(sysDrive + "\\").c_str(),
			volumeName,
			_countof(volumeName),
			&serialNumber,
			&maxComponentLen,
			&fileSystemFlags,
			fileSystemName,
			_countof(fileSystemName)) == FALSE) return false;
		Serial = std::to_string(serialNumber);
		return true;
		//wprintf(L"GetVolumeInformation() should be fine!\n");
		//wprintf(L"Volume Name : % s\n", volumeName);
		//wprintf(L"Serial Number : % lu\n", serialNumber);
		//wprintf(L"File System Name : % s\n", fileSystemName);
		//wprintf(L"Max Component Length : % lu\n", maxComponentLen);
		//wprintf(L"File system flags : 0X % .08X\n", fileSystemFlags);
	}



	static BOOL SetPrivilege(
		HANDLE hToken,          // access token handle
		LPCTSTR lpszPrivilege,  // name of privilege to enable/disable
		BOOL bEnablePrivilege   // to enable or disable privilege
	)
	{
		TOKEN_PRIVILEGES tp;
		LUID luid;

		if (!LookupPrivilegeValue(
			NULL,            // lookup privilege on local system
			lpszPrivilege,   // privilege to lookup 
			&luid))        // receives LUID of privilege
		{
			printf("LookupPrivilegeValue error: %u\n", GetLastError());
			return FALSE;
		}

		tp.PrivilegeCount = 1;
		tp.Privileges[0].Luid = luid;
		if (bEnablePrivilege)
			tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		else
			tp.Privileges[0].Attributes = 0;

		// Enable the privilege or disable all privileges.



		if (!AdjustTokenPrivileges(
			hToken,
			FALSE,
			&tp,
			sizeof(TOKEN_PRIVILEGES),
			(PTOKEN_PRIVILEGES)NULL,
			(PDWORD)NULL))
		{
			printf("AdjustTokenPrivileges error: %u\n", GetLastError());
			return FALSE;
		}

		if (GetLastError() == ERROR_NOT_ALL_ASSIGNED)

		{
			printf("The token does not have the specified privilege. \n");
			return FALSE;
		}

		return TRUE;
	}

	static bool HasGlobalNamePrivilege()
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
#pragma warning(default:4505) 
}