/**
 * Purpose      : A class that wraps the Windows Service Program init/control/exit
				  sequences.
 */
#pragma once

#include <windows.h>
#include <string>
#include <winsvc.h>
#include <stdarg.h>
#include <dbt.h>
#include <crtdbg.h>
#include <vector>
#include <thread>
#include <functional>
#include <shellapi.h>
#include <LM.h>
#include <NTSecAPI.h>
#include <random>
#include <userenv.h>
#include <sddl.h>
#include <AclAPI.h>
#include <NetSh.h>
#include "../LibLogging/LibLogging.h"
#include "../Common/sys_utils.h"

#pragma comment(lib, "netapi32.lib")
#pragma comment(lib, "Userenv.lib")

static  BOOL SetPrivilege(
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

class PolicyClass
{
private:
	LSA_HANDLE handle = nullptr;
	const std::wstring systemName;
	void
		InitLsaString(
			PLSA_UNICODE_STRING LsaString,
			LPWSTR String
		)
	{
		DWORD StringLength;

		if (String == NULL) {
			LsaString->Buffer = NULL;
			LsaString->Length = 0;
			LsaString->MaximumLength = 0;
			return;
		}

		StringLength = lstrlenW(String);
		LsaString->Buffer = String;
		LsaString->Length = (USHORT)StringLength * sizeof(WCHAR);
		LsaString->MaximumLength = (USHORT)(StringLength + 1) * sizeof(WCHAR);
	}

public:
	PolicyClass(const std::wstring& SystemName) :systemName(SystemName)
	{
		LSA_UNICODE_STRING sys{};
		sys.Buffer = const_cast<PWSTR>(systemName.c_str());
		sys.Length = static_cast<USHORT>(systemName.size());
		sys.MaximumLength = static_cast<USHORT>(systemName.capacity());

		LSA_OBJECT_ATTRIBUTES ObjectAttributes{};
		if (const auto res = LsaOpenPolicy(systemName.empty() ? nullptr : &sys, &ObjectAttributes, POLICY_ALL_ACCESS, &handle); res != NOERROR)
		{
			Log::error(__func__, "LsaOpenPolicy returned NTSTATUS 0x{:x}", static_cast<uint32_t>(res));
			handle = nullptr; return;
		}
	}
	~PolicyClass()
	{
		if (handle != nullptr)
		{
			LsaClose(handle);
			handle = nullptr;
		}
	}


	bool TakeFileOwnership(const std::wstring& fileName, const std::wstring& userName)
	{

		bool bRetval = false;

		HANDLE hToken = NULL;
		//PSID pSIDAdmin = NULL;
		PSID pSIDEveryone = NULL;
		PACL pACL = NULL;
		SID_IDENTIFIER_AUTHORITY SIDAuthWorld =
			SECURITY_WORLD_SID_AUTHORITY;
		SID_IDENTIFIER_AUTHORITY SIDAuthNT = SECURITY_NT_AUTHORITY;
		const int NUM_ACES = 2;
		EXPLICIT_ACCESS ea[NUM_ACES];
		DWORD dwRes;

		std::vector<uint8_t> userSid;
		if (!GetAccountSid(userName, userSid)) return false;

		// Specify the DACL to use.
		// Create a SID for the Everyone group.
		if (!AllocateAndInitializeSid(&SIDAuthWorld, 1,
			SECURITY_WORLD_RID,
			0,
			0, 0, 0, 0, 0, 0,
			&pSIDEveryone))
		{
			printf("AllocateAndInitializeSid (Everyone) error %u\n",
				GetLastError());
			goto Cleanup;
		}

		

		


		ZeroMemory(&ea, NUM_ACES * sizeof(EXPLICIT_ACCESS));

		// Set read access for Everyone.
		ea[0].grfAccessPermissions = GENERIC_READ;
		ea[0].grfAccessMode = SET_ACCESS;
		ea[0].grfInheritance = NO_INHERITANCE;
		ea[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
		ea[0].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
		ea[0].Trustee.ptstrName = (LPWCH)pSIDEveryone;

		// Set full control for Administrators.
		ea[1].grfAccessPermissions = GENERIC_ALL;
		ea[1].grfAccessMode = SET_ACCESS;
		ea[1].grfInheritance = NO_INHERITANCE;
		ea[1].Trustee.TrusteeForm = TRUSTEE_IS_SID;
		ea[1].Trustee.TrusteeType = TRUSTEE_IS_GROUP;
		ea[1].Trustee.ptstrName = (LPWCH)userSid.data();

		if (ERROR_SUCCESS != SetEntriesInAclW(NUM_ACES,
			ea,
			NULL,
			&pACL))
		{
			printf("Failed SetEntriesInAcl\n");
			goto Cleanup;
		}

		// Try to modify the object's DACL.
		dwRes = SetNamedSecurityInfoW(
			(LPWSTR)fileName.c_str(),                 // name of the object
			SE_FILE_OBJECT,              // type of object
			DACL_SECURITY_INFORMATION,   // change only the object's DACL
			nullptr, nullptr,                  // do not change owner or group
			pACL,                        // DACL specified
			nullptr);                       // do not change SACL

		if (ERROR_SUCCESS == dwRes)
		{
			printf("Successfully changed DACL\n");
			bRetval = true;
			// No more processing needed.
			goto Cleanup;
		}
		if (dwRes != ERROR_ACCESS_DENIED)
		{
			printf("First SetNamedSecurityInfo call failed: %u\n",
				dwRes);
			goto Cleanup;
		}

		//// If the preceding call failed because access was denied, 
		//// enable the SE_TAKE_OWNERSHIP_NAME privilege, create a SID for 
		//// the Administrators group, take ownership of the object, and 
		//// disable the privilege. Then try again to set the object's DACL.

		//// Open a handle to the access token for the calling process.
		//if (!OpenProcessToken(GetCurrentProcess(),
		//	TOKEN_ADJUST_PRIVILEGES,
		//	&hToken))
		//{
		//	printf("OpenProcessToken failed: %u\n", GetLastError());
		//	goto Cleanup;
		//}

		//// Enable the SE_TAKE_OWNERSHIP_NAME privilege.
		//if (!SetPrivilege(hToken, SE_TAKE_OWNERSHIP_NAME, TRUE))
		//{
		//	printf("You must be logged on as Administrator.\n");
		//	goto Cleanup;
		//}

		//// Set the owner in the object's security descriptor.
		//dwRes = SetNamedSecurityInfo(
		//	lpszOwnFile,                 // name of the object
		//	SE_FILE_OBJECT,              // type of object
		//	OWNER_SECURITY_INFORMATION,  // change only the object's owner
		//	pSIDAdmin,                   // SID of Administrator group
		//	NULL,
		//	NULL,
		//	NULL);

		//if (dwRes != ERROR_SUCCESS)
		//{
		//	printf("Could not set owner. Error: %u\n", dwRes);
		//	goto Cleanup;
		//}

		//// Disable the SE_TAKE_OWNERSHIP_NAME privilege.
		//if (!SetPrivilege(hToken, SE_TAKE_OWNERSHIP_NAME, FALSE))
		//{
		//	printf("Failed SetPrivilege call unexpectedly.\n");
		//	goto Cleanup;
		//}

		//// Try again to modify the object's DACL,
		//// now that we are the owner.
		//dwRes = SetNamedSecurityInfo(
		//	lpszOwnFile,                 // name of the object
		//	SE_FILE_OBJECT,              // type of object
		//	DACL_SECURITY_INFORMATION,   // change only the object's DACL
		//	NULL, NULL,                  // do not change owner or group
		//	pACL,                        // DACL specified
		//	NULL);                       // do not change SACL

		//if (dwRes == ERROR_SUCCESS)
		//{
		//	printf("Successfully changed DACL\n");
		//	bRetval = TRUE;
		//}
		//else
		//{
		//	printf("Second SetNamedSecurityInfo call failed: %u\n",
		//		dwRes);
		//}
		bRetval = false;
	Cleanup:

		
		if (pSIDEveryone)
			FreeSid(pSIDEveryone);

		if (pACL)
			LocalFree(pACL);

		if (hToken)
			CloseHandle(hToken);

		return bRetval;

	}

	static std::wstring GenerateRandomPassword(size_t passwordLen)
	{
		std::wstring chars = L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789%^&$+-_!@#%?{}[]~";
		std::wstring pw{};
	

		const size_t from = 0;
		const size_t to = chars.size() - 1;
		std::random_device rd;
		std::mt19937 g(rd());
		std::uniform_int_distribution<size_t>  distr(from, to);
		auto rng = std::default_random_engine{};
		std::shuffle(chars.begin(), chars.end(), rng);
		for (size_t i = 0; i < passwordLen; ++i) {
			pw.push_back(chars[distr(g)]);
		}
		return pw;
	}


	bool LocalGroupAddMember(const std::wstring& groupName, const std::vector<uint8_t>& sid)
	{
		if (groupName.empty()) return false;
		LOCALGROUP_MEMBERS_INFO_0 i0{};
		i0.lgrmi0_sid = (PSID)sid.data();
		if (const auto res = NetLocalGroupAddMembers(systemName.empty() ? nullptr : systemName.c_str(), 
			groupName.c_str(), 0, (LPBYTE)&i0, 1); res != NERR_Success)
		{
			if (res == ERROR_MEMBER_IN_ALIAS)
			{
				return true;
			}
			Log::error(__func__, "NetLocalGroupAddMember failed 0x{:x}", res);
			return false;
		}
		return true;
	}
	std::vector<std::wstring> GetServerLocalGroups()
	{
		std::vector<std::wstring> result{};
		LPBYTE buf = nullptr;
		DWORD totalEntries = 0;
		DWORD entriesRead = 0;
		PDWORD_PTR resumeHandle = nullptr;
		if (const auto res = NetLocalGroupEnum(systemName.empty() ? nullptr : systemName.c_str(), 0, &buf, MAX_PREFERRED_LENGTH, &entriesRead, &totalEntries, resumeHandle); res == 0)
		{
			auto p = reinterpret_cast<LOCALGROUP_INFO_0*>(buf);
			for (size_t i = 0; i < entriesRead; i++)
			{
				if (p->lgrpi0_name == nullptr) continue;
				result.push_back(p->lgrpi0_name);
				p++;
			}
		}
		if (buf != nullptr)
		{
			NetApiBufferFree(buf);
		}

		return result;
	}

	LPWSTR GetServerName() const
	{
		return systemName.empty() ? nullptr : (LPWSTR)systemName.c_str();
	}

	bool SetUserPassword(const std::wstring& userName, const std::wstring& password, const std::wstring& comment)
	{
		if (userName.empty()) return false;
		if (password.size() < 24) {
			Log::error(__func__, "Password MUST be at least 24 symbols");
			return false;
		}

	

		USER_INFO_1 ui1{};
		ui1.usri1_name = (LPWSTR)userName.c_str();
		ui1.usri1_password = (LPWSTR)password.c_str();
		ui1.usri1_priv = USER_PRIV_USER;
		ui1.usri1_home_dir = nullptr;
		ui1.usri1_comment = (LPWSTR)comment.c_str();
		ui1.usri1_flags = UF_SCRIPT | UF_PASSWD_CANT_CHANGE | UF_DONT_EXPIRE_PASSWD;
		ui1.usri1_script_path = nullptr;

		DWORD err = 0;
		if (const auto res = NetUserSetInfo(GetServerName(), userName.c_str(),
			1, (LPBYTE)&ui1, &err); res != NERR_Success)
		{
			
			return false;
		}

		
		return true;
	}


	bool DelUser(const std::wstring& userName)
	{


		std::vector<uint8_t> sid;
		if (GetAccountSid(userName, sid))
		{
			if (systemName.empty())
			{
				LPWSTR sidStr = nullptr;
				if (FALSE != ConvertSidToStringSidW(sid.data(), &sidStr))
				{
					if (FALSE == DeleteProfileW(sidStr, nullptr, nullptr))
					{
						Log::error(__func__, "Delete Profile Failed {}", GetLastError());
					}
					LocalFree(sidStr);

				}
			}
			if (const auto res = NetUserDel(systemName.empty() ? nullptr : systemName.c_str(), userName.c_str()); res != 0)
			{
				Log::error(__func__, "NetUserDel failed. Error 0x{:x}", static_cast<uint32_t>(res));
				return false;
			}


		}




		return true;
	}


	bool AddUser(const std::wstring& userName, const std::wstring& password, const std::wstring& comment)
	{
		DWORD dwLevel = 1;
		DWORD dwError = 0;
		USER_INFO_1 ui{};

		ui.usri1_name = (LPWSTR)userName.c_str();
		ui.usri1_password = (LPWSTR)password.c_str();
		ui.usri1_priv = USER_PRIV_USER;
		ui.usri1_home_dir = nullptr;
		ui.usri1_comment = (LPWSTR)comment.c_str();
		ui.usri1_flags = UF_SCRIPT | UF_PASSWD_CANT_CHANGE | UF_DONT_EXPIRE_PASSWD;
		ui.usri1_script_path = nullptr;
		//
		// Call the NetUserAdd function, specifying level 1.
		//
		const auto nStatus = NetUserAdd(systemName.empty() ? nullptr : systemName.c_str(), dwLevel, (LPBYTE)&ui, &dwError);
		if (nStatus != 0)
		{
			Log::error(__func__, "NetUserAdd Result 0x{:x}, dwError 0x{:x}", static_cast<uint32_t>(nStatus),
				static_cast<uint32_t>(dwError));
		}
		return (nStatus == 0);
	}

	bool getAccountStringSid(const std::wstring& AccountName, std::wstring & sidString)
	{
		bool res = false;
		std::vector<uint8_t> sid;
		if (!GetAccountSid(AccountName, sid)) return false;
		LPWSTR sidStr = nullptr;
		if (FALSE != ConvertSidToStringSidW(sid.data(), &sidStr))
		{
			sidString = sidStr;
			res = true;
		}
		if (sidStr) LocalFree(sidStr);
		return res;
	}
	bool GetAccountSid(const std::wstring& AccountName, std::vector<uint8_t>& Sid)
	{
		std::vector<wchar_t> ReferencedDomain; ReferencedDomain.resize(16);
		DWORD cchReferencedDomain = static_cast<DWORD>(ReferencedDomain.size()); // initial allocation size
		SID_NAME_USE peUse;
		BOOL bSuccess = FALSE; // assume this function will fail


		Sid.resize(128);

		DWORD cbSid = static_cast<DWORD>(Sid.size());    // initial allocation attempt


			//
			// Obtain the SID of the specified account on the specified system.
			//
		while (!LookupAccountNameW(
			systemName.c_str(),         // machine to lookup account on
			AccountName.c_str(),        // account to lookup
			Sid.data(),               // SID of interest
			&cbSid,             // size of SID
			ReferencedDomain.data(),   // domain account was found on
			&cchReferencedDomain,
			&peUse
		)) {
			const auto gle = GetLastError();
			if (gle == ERROR_INSUFFICIENT_BUFFER) {

				if (cbSid != Sid.size())
				{
					Sid.resize(cbSid);
				}

				if (cchReferencedDomain != ReferencedDomain.size())
				{
					ReferencedDomain.resize(cchReferencedDomain);
				}
			}
			else {
				Log::error(__func__, "LookupAccountNameW returned 0x{:x}", static_cast<uint32_t>(gle));
				return false;
			}
		}
		return true;
	}

	bool SetPrivilegeOnAccount(
		const std::vector<uint8_t>& AccountSid,            // SID to grant privilege to
		LPWSTR PrivilegeName,       // privilege to grant (Unicode)
		BOOL bEnable                // enable or disable
	)
	{

		constexpr auto  STATUS_OBJECT_NAME_NOT_FOUND_ = ((NTSTATUS)0xC0000034L);

		if (handle == nullptr) {
			Log::error(__func__, " Handle is nullptr");
			return false;
		}
		LSA_UNICODE_STRING PrivilegeString;

		//
		// Create a LSA_UNICODE_STRING for the privilege name.
		//
		InitLsaString(&PrivilegeString, PrivilegeName);

		//
		// grant or revoke the privilege, accordingly
		//

		NTSTATUS res = 0;
		if (bEnable) {
			res = LsaAddAccountRights(
				handle,       // open policy handle
				(PSID)AccountSid.data(),         // target SID
				&PrivilegeString,   // privileges
				1                   // privilege count
			);
		}
		else {
			res = LsaRemoveAccountRights(
				handle,       // open policy handle
				(PSID)AccountSid.data(),         // target SID
				FALSE,              // do not disable all rights
				&PrivilegeString,   // privileges
				1                   // privilege count
			);
		}


		if (res != 0)
		{
			if ((res == STATUS_OBJECT_NAME_NOT_FOUND_) && (!bEnable))
			{
				return true;
			}

			Log::error(__func__, "Res is 0x{:x}", static_cast<uint32_t>(res));

		}
		return res == 0;
	}



	LSA_HANDLE get() const
	{
		return handle;

	}


	bool GetGroupNameFromSID(const std::wstring& sidTxt, std::wstring& group, std::wstring & refDomain)
	{
		PSID psid = nullptr;
		SID_NAME_USE SidType = SidTypeWellKnownGroup;
		BOOL bSucceeded = ConvertStringSidToSidW(sidTxt.c_str(), &psid);
		if (bSucceeded == FALSE) {
			Log::error(__func__, "ConvertStringSidToSidW failed {}", static_cast<uint32_t>(GetLastError()));
			return false;
		}
		std::wstring buf; buf.resize(128);
		DWORD sz = static_cast<DWORD>(buf.size());
		std::wstring dbuf; dbuf.resize(128);
		DWORD dbufSz = static_cast<DWORD>(dbuf.size());
		bool bres = false;
		if (LookupAccountSidW(systemName.empty() ? nullptr : systemName.c_str(), psid, (LPWSTR)buf.c_str(), &sz, (LPWSTR)dbuf.c_str(), &dbufSz, &SidType))
		{
			buf.resize(sz);
			dbuf.resize(dbufSz);
			group = buf;
			refDomain = dbuf;
			bres = true;
			
		}
		else
		{
			Log::error(__func__, "LookupAccountSidW failed '{}' '{}'",GetLastError() ,pbn::to_utf8(sidTxt));
		}


		LocalFree(psid);
		return bres;
	}
};


class TConsoleService {


	TConsoleService(const TConsoleService&) = delete;
	TConsoleService& operator=(const TConsoleService&) = delete;

public:
	TConsoleService()
	{
		hEventQuit_ = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);
		_ASSERTE(hEventQuit_ != nullptr);

		status_.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
		status_.dwCurrentState = SERVICE_STOPPED;
		// SERVICE_ACCEPT_STOP will be added when switching from 
		// STOP_PENDING to RUNNING state.
		status_.dwControlsAccepted = 0;
		status_.dwWin32ExitCode = 0;
		status_.dwServiceSpecificExitCode = 0;
		status_.dwCheckPoint = 0;
		status_.dwWaitHint = 0;

		if (pbn::cpu::CPU::GroupsCount() > 1)
		{
			//In systems with many groups (threads > 64) Group 0 contains 64 threads, Groups > 0 might have less resources
			pbn::cpu::CPU::SetThreadGroupAffinity(0);
		}
	}



	static TConsoleService* GetInstance()
	{
		static TConsoleService SINGLETON;
		return &SINGLETON;
	};

	static bool CPUOK()
	{
		return pbn::cpu::CheckCPUOK();
	};


	static bool HasGlobalNamePrivilege()
	{
		return pbn::OS::HasGlobalNamePrivilege();
	};
	
	static void SetLogging()
	{
		
	Log::SetAsyncLogging(true);
	Log::SetConsoleLogging(false);
	Log::SetODSLogging(true);
		

	pbn::fs::Path actual_app_path(pbn::OS::get_current_module_path());
	actual_app_path.append(L"logtofile");
	Log::SetFileLogging(pbn::fs::file_exist(actual_app_path));
	}


	static bool SetServicePrivilegies(PolicyClass& policy, const std::vector<uint8_t>& sid)
	{

		if (!policy.SetPrivilegeOnAccount(sid, (LPWSTR)SE_DENY_BATCH_LOGON_NAME, true)) return false;
		if (!policy.SetPrivilegeOnAccount(sid, (LPWSTR)SE_BATCH_LOGON_NAME, false)) return false;

		if (!policy.SetPrivilegeOnAccount(sid, (LPWSTR)SE_DENY_INTERACTIVE_LOGON_NAME, true)) return false;
		if (!policy.SetPrivilegeOnAccount(sid, (LPWSTR)SE_INTERACTIVE_LOGON_NAME, false)) return false;

		if (!policy.SetPrivilegeOnAccount(sid, (LPWSTR)SE_DENY_NETWORK_LOGON_NAME, true)) return false;
		if (!policy.SetPrivilegeOnAccount(sid, (LPWSTR)SE_NETWORK_LOGON_NAME, false)) return false;

		if (!policy.SetPrivilegeOnAccount(sid, (LPWSTR)SE_DENY_REMOTE_INTERACTIVE_LOGON_NAME, true)) return false;
		if (!policy.SetPrivilegeOnAccount(sid, (LPWSTR)SE_REMOTE_INTERACTIVE_LOGON_NAME, false)) return false;

		if (!policy.SetPrivilegeOnAccount(sid, (LPWSTR)SE_SERVICE_LOGON_NAME, true)) return false;
		if (!policy.SetPrivilegeOnAccount(sid, (LPWSTR)SE_DENY_SERVICE_LOGON_NAME, false)) return false;

		if (!policy.SetPrivilegeOnAccount(sid, (LPWSTR)SE_TIME_ZONE_NAME, true)) return false;
		if (!policy.SetPrivilegeOnAccount(sid, (LPWSTR)SE_SYSTEMTIME_NAME, true)) return false;
		if (!policy.SetPrivilegeOnAccount(sid, (LPWSTR)SE_CREATE_GLOBAL_NAME, true)) return false;

		//if (!policy.SetPrivilegeOnAccount(sid, (LPWSTR)SE_SYSTEM_PROFILE_NAME, true)) return false;
		

		
		std::wstring groupName, refDomain;

		//Performance Monitor Users group
		if (policy.GetGroupNameFromSID(L"S-1-5-32-558", groupName, refDomain))
		{
			if (!policy.LocalGroupAddMember(groupName, sid)) return false;
		}
		

		//Users group

		if (policy.GetGroupNameFromSID(L"S-1-5-32-545", groupName, refDomain))
		{
			if (!policy.LocalGroupAddMember(groupName, sid)) return false;
		}

		return true;
	}

	static bool UpdateOrCreateServiceUser(const std::wstring& userName, const std::wstring& password, 
		const std::wstring& comment)
	{
		if (userName.empty()) return false;
		PolicyClass policy(L"");
		if (policy.get() == nullptr) return false;
		std::vector<uint8_t> sid;
		if (policy.GetAccountSid(userName, sid))
		{
			
			if (!policy.SetUserPassword(userName, password, comment))
			{
				return false;
			}
		}
		else
		{
			if (!policy.AddUser(userName, password, comment)) return false;
			if (!policy.GetAccountSid(userName, sid)) return false;
		}
		return SetServicePrivilegies(policy, sid);
	}


	//if ServiceUser  is empty string - LocalSystem account is used
	//if you don't need to do anything prior service start when service is installed set preInstallStart = nullptr
	//preInstallStart should be used for services which have httpServer2 to register url for the user
	//appMain is the actual mainThread fn. Please check for hQuit and don't block the exit
	static void init(const std::wstring& ServiceName,
		const std::wstring& ServiceDescription,
		const std::wstring& ServiceUser,
		const std::vector<std::wstring>& dependentOnSvcs,
		std::function<bool(const std::wstring& serviceUser)> preInstallStart,
		std::function<DWORD(HANDLE hQuit, const bool bConsole)> appMain)
	{
		auto inst = GetInstance();
		inst->serviceName_ = ServiceName;
		inst->serviceDescription_ = ServiceDescription;
		inst->dependentOnSvcs_ = dependentOnSvcs;
		inst->appMain_ = appMain;
		inst->serviceUser_ = ServiceUser;
		inst->preInstallStart_ = preInstallStart;
	}

	int SvcInstall()
	{
		Log::SetAsyncLogging(false);
		Log::SetConsoleLogging(true);
		SvcRemove();

		LPCWSTR localUserPtr = nullptr;
		LPCWSTR localPwdPtr = nullptr;
		auto localUser = L".\\" + serviceUser_;
		std::wstring pw = PolicyClass::GenerateRandomPassword(32);
		if (!serviceUser_.empty())
		{

			if (!TConsoleService::UpdateOrCreateServiceUser(serviceUser_, pw, L"Service user"))
			{
				Log::error(__func__, "Can't create user");
			}

			localUserPtr = localUser.c_str();
			localPwdPtr = pw.c_str();
		}







		SC_HANDLE schSCManager;
		SC_HANDLE schService;
		std::wstring Path;
		Path.resize(32768);

		auto sz = GetModuleFileNameW(nullptr, (LPWSTR)Path.c_str(), (DWORD)Path.size());

		if (sz == 0)
		{
			printf("Cannot install service (%d)\n", GetLastError());
			return 1;
		}

		Path.resize(sz);
		// Get a handle to the SCM database. 

		schSCManager = OpenSCManager(
			nullptr,                    // local computer
			nullptr,                    // ServicesActive database 
			SC_MANAGER_ALL_ACCESS);  // full access rights 

		if (nullptr == schSCManager)
		{
			printf("OpenSCManager failed (%d)\n", GetLastError());
			return 1;
		}

		// Create the service

		std::vector<wchar_t> depends;

		std::back_insert_iterator< std::vector<wchar_t> > back_it(depends);
		for (const auto& str : dependentOnSvcs_)
		{
			std::copy(str.begin(), str.end(), back_it);
			depends.push_back(L'\0');
		}
		depends.push_back(L'\0');
		depends.push_back(L'\0');





		schService = CreateServiceW(
			schSCManager,              // SCM database 
			serviceName_.c_str(),                   // name of service 
			serviceName_.c_str(),                   // service name to display 
			SERVICE_ALL_ACCESS,        // desired access 
			SERVICE_WIN32_OWN_PROCESS, // service type 
			SERVICE_AUTO_START,      // start type 
			SERVICE_ERROR_NORMAL,      // error control type 
			Path.c_str(),                    // path to service's binary 
			nullptr,                      // no load ordering group 
			nullptr,                      // no tag identifier 
			depends.data(),                      // no dependencies 
			localUserPtr,                      // LocalSystem account 
			localPwdPtr);                     // no password 

		if (schService == nullptr)
		{

			schService = OpenServiceW(
				schSCManager,              // SCM database 
				serviceName_.c_str(),                   // name of service 			
				SERVICE_ALL_ACCESS        // desired access 
			);                     // no password 



			if (schService == nullptr)
			{
				printf("CreateService failed (%d)\n", GetLastError());
				CloseServiceHandle(schSCManager);
				return 1;
			}


		}
		else printf("Service installed successfully\n");

		SERVICE_DESCRIPTION desc{};
		desc.lpDescription = const_cast<LPWSTR>(serviceDescription_.c_str());

		ChangeServiceConfig2W(
			schService,
			SERVICE_CONFIG_DESCRIPTION,
			&desc
		);




		SC_ACTION sc_action{};
		sc_action.Delay = 0;
		sc_action.Type = SC_ACTION_RESTART;


		SERVICE_FAILURE_ACTIONS failure_actions{};
		failure_actions.dwResetPeriod = 1; //1second to work before reset error counter
		failure_actions.lpRebootMsg = nullptr;
		failure_actions.lpCommand = nullptr;
		failure_actions.cActions = 1;
		failure_actions.lpsaActions = &sc_action;



		ChangeServiceConfig2W(
			schService,
			SERVICE_CONFIG_FAILURE_ACTIONS,
			&failure_actions
		);

		bool bFailed = false;
		if (preInstallStart_ != nullptr)
		{
			bFailed = !preInstallStart_(serviceUser_);
		}

		if (!bFailed)
		{
			if (TRUE == StartServiceW(schService, 0, nullptr))
			{
				printf("Service Started\n");
			}
		}

		CloseServiceHandle(schService);
		CloseServiceHandle(schSCManager);
		return bFailed?1: 0;
	}


	int SvcRemove()
	{
		Log::SetAsyncLogging(false);
		Log::SetConsoleLogging(true);




		SC_HANDLE schSCManager;
		SC_HANDLE schService;
		std::wstring Path;
		Path.resize(32768);

		auto sz = GetModuleFileNameW(nullptr, (LPWSTR)Path.c_str(), (DWORD)Path.size());

		if (sz == 0)
		{
			printf("GetModuleFileNameW failed (%d)\n", GetLastError());
			return 1;
		}

		Path.resize(sz);
		// Get a handle to the SCM database. 

		schSCManager = OpenSCManager(
			nullptr,                    // local computer
			nullptr,                    // ServicesActive database 
			SC_MANAGER_ALL_ACCESS);  // full access rights 

		if (nullptr == schSCManager)
		{
			printf("OpenSCManager failed (%d)\n", GetLastError());
			return 1;
		}

		// Create the service

		schService = OpenService(schSCManager, serviceName_.c_str(), SERVICE_ALL_ACCESS);

		if (schService == nullptr)
		{
			printf("OpenService failed (%d)\n", GetLastError());
			CloseServiceHandle(schSCManager);
			return 2;
		}
		else
		{
			bool bShouldStop = true;
			SERVICE_STATUS ss{};
			if (QueryServiceStatus(schService, &ss))
			{
				if (ss.dwCurrentState == SERVICE_STOPPED) bShouldStop = false;
			}

			if (bShouldStop)
			{
				SERVICE_CONTROL_STATUS_REASON_PARAMS ssp{};
				ssp.dwReason = SERVICE_STOP_REASON_FLAG_PLANNED | SERVICE_STOP_REASON_MINOR_NONE | SERVICE_STOP_REASON_MAJOR_NONE;
				ssp.pszComment = (LPWSTR)L"Service remove";

				if (!ControlServiceExW(
					schService,
					SERVICE_CONTROL_STOP,
					SERVICE_CONTROL_STATUS_REASON_INFO,
					&ssp))
				{

					printf("Can not stop the service (%u)\n", GetLastError());
				}

				SERVICE_STATUS ss{};
				while (QueryServiceStatus(schService, &ss))
				{
					if (ss.dwCurrentState == SERVICE_STOPPED) break;
					Log::info(__func__, "Wait to Stop");
					std::this_thread::sleep_for(std::chrono::milliseconds(500));
					ss = {};

				}
			}


			if (DeleteService(schService))
			{
				printf("Service removed successfully\n");
			}
			else
			{
				printf("Service remove failed\n");
			}
		}


		CloseServiceHandle(schService);
		CloseServiceHandle(schSCManager);
		return 0;
	}
	// start the service, to be called from _tmain
	static DWORD start() throw()
	{
		Log::SetAsyncLogging(true);
		auto inst = GetInstance();
		// Parse commandline arguments to detect '/console'. If specified, this
		// indicates the service to be run as a console program, usually for
		// debugging purposes.
		int nArgs = 0;
		 LPWSTR* alpszArgs = ::CommandLineToArgvW(::GetCommandLineW(), &nArgs);



		bool bServiceMode = false;
		DWORD SessionID = -1;
		DWORD Size = 0;
		HANDLE hToken = nullptr;
		if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
		{
			::LocalFree(alpszArgs);
			return 1;
		}

		if (!GetTokenInformation(hToken, TokenSessionId, &SessionID, sizeof(SessionID), &Size) || !Size)
		{
			::LocalFree(alpszArgs);
			return 1;
		}
		if (SessionID == 0)
		{
			bServiceMode = true;
		}

		CloseHandle(hToken);



		if (!bServiceMode)
		{
			for (int i = 0; i < nArgs; i++) {
				LPWSTR lpszArg = alpszArgs[i];
				if (lpszArg[0] == L'/' || lpszArg[0] == L'-')
				{
					if (::_wcsicmp(&lpszArg[1], L"console") == 0) {

						inst->fConsoleMode = true;
						continue;
					}

					if (::_wcsicmp(&lpszArg[1], L"install") == 0) {
						::LocalFree(alpszArgs);
						return inst->SvcInstall();
					}

					if (::_wcsicmp(&lpszArg[1], L"remove") == 0) {
						::LocalFree(alpszArgs);
						return inst->SvcRemove();
					}
				}
			}

			if (!inst->fConsoleMode)
			{

				printf("<your application>\n");
				printf("---------------------------\n");
				printf("params:\n");
				printf("\t-install - to install the service\n");
				printf("\t-remove - to remove the service\n\n");
				printf("\t-console - to run as console\n\n");
				::LocalFree(alpszArgs);
				return 0;

			}
		}
		if (inst->fConsoleMode) {
			Log::SetConsoleLogging(true);
			// console program mode
			TConsoleService::_serviceMain((DWORD)nArgs, alpszArgs);
		}
		else {
			// service mode
			SERVICE_TABLE_ENTRYW st[] =
			{   // szServiceName_ is ignored for SERVICE_OWN_PROCESS, but we add it anyhow.
				{ (LPWSTR)inst->serviceName_.c_str(), _serviceMain },
				{ nullptr, nullptr }
			};
			if (::StartServiceCtrlDispatcherW(st) == 0)
				inst->status_.dwWin32ExitCode = GetLastError();
		}

		// don't forget to free the memory allocated by CommandLinetoArgvW()!
		::LocalFree(alpszArgs);

		Log::LogEventToFileSync(LOGLEVEL::Info, LOGTYPE::Audit, __func__, "ServiceStopped");
		return inst->status_.dwWin32ExitCode;
	}

	void serviceMain(DWORD dwArgc, LPTSTR* lpszArgv)
	{
		// Register the control request handler
		status_.dwCurrentState = SERVICE_START_PENDING;
		if (isDebugMode()) {
			// Register a console Ctrl+Break handler
			::SetConsoleCtrlHandler(TConsoleService::_consoleCtrlHandler, TRUE);
			_putws(L"Press Ctrl+C or Ctrl+Break to quit...");
		}
		else {
			// register the service handler routine
			hServiceStatus_ = ::RegisterServiceCtrlHandlerEx(
				serviceName_.c_str(),
				(LPHANDLER_FUNCTION_EX)_serviceControlHandlerEx,
				(LPVOID)this);
			if (hServiceStatus_ == nullptr) {
				//LogEvent(_T("Handler not installed"));
				return;
			}
		}
		setServiceStatus(SERVICE_START_PENDING);

		status_.dwWin32ExitCode = S_OK;
		status_.dwCheckPoint = 0;
		status_.dwWaitHint = 0;

		// When the Run function returns, the service has stopped.
		status_.dwWin32ExitCode = run();

		setServiceStatus(SERVICE_STOPPED);
	}


	DWORD WaitForQuitEvent()
	{
		::WaitForSingleObject(hEventQuit_, INFINITE);
		return 0;
	}

	// override this in your derived class to implement your own service's
	// initialization code.
	virtual DWORD run()
	{
		setServiceStatus(SERVICE_RUNNING);

		// Wait for the quit event to be signalled. Event would be signalled either from 
		// the SERVICE_STOP control handler or from the Ctrl+C control handler.

		Log::LogEventToFileSync(LOGLEVEL::Info, LOGTYPE::Audit, __func__, "ServiceStart");
		DWORD res = appMain_ == nullptr ? WaitForQuitEvent() : appMain_(hEventQuit_, fConsoleMode);
		Log::LogEventToFileSync(LOGLEVEL::Info, LOGTYPE::Audit, __func__, "Main end");
		return res;
	}

	/* is '/debug' commandline option specified? */
	bool isDebugMode()
	{
		return fConsoleMode;
	}





	/**
	 * Various control command handlers, should be self-explanatory
	 */
	virtual void onStop() throw()
	{
		Log::LogEventToFileSync(LOGLEVEL::Info, LOGTYPE::Audit, __func__, "StopEvent");
		setServiceStatus(SERVICE_STOP_PENDING);
		::SetEvent(hEventQuit_);
	}

	virtual void onPause() throw()
	{
		Log::LogEventToFileSync(LOGLEVEL::Info, LOGTYPE::Audit, __func__, "PauseEvent");
	}

	virtual void onContinue() throw()
	{
		Log::LogEventToFileSync(LOGLEVEL::Info, LOGTYPE::Audit, __func__, "ContinueEvent");
	}

	virtual void onInterrogate() throw()
	{
		Log::LogEventToFileSync(LOGLEVEL::Info, LOGTYPE::Audit, __func__, "InterrogateEvent");
	}


	virtual DWORD onPreShutdown(LPSERVICE_PRESHUTDOWN_INFO pInfo)
	{
		Log::LogEventToFileSync(LOGLEVEL::Info, LOGTYPE::Audit, __func__, "PreShutdownEvent");
		setServiceStatus(SERVICE_STOP_PENDING);
		::SetEvent(hEventQuit_);
		return NO_ERROR;
	}

	virtual void onShutdown() throw()
	{
		Log::LogEventToFileSync(LOGLEVEL::Info, LOGTYPE::Audit, __func__, "ShutdownEvent");
		setServiceStatus(SERVICE_STOP_PENDING);
		::SetEvent(hEventQuit_);
	}

	virtual DWORD onDeviceEvent(DWORD dwDBT, PDEV_BROADCAST_HDR pHdr)
	{
		Log::LogEventToFileSync(LOGLEVEL::Info, LOGTYPE::Audit, __func__, "DeviceEvent");
		return NO_ERROR;
	}

	virtual DWORD onHardwareProfileChange(DWORD dwDBT)
	{
		Log::LogEventToFileSync(LOGLEVEL::Info, LOGTYPE::Audit, __func__, "HardwareProfileChangeEvent");
		return NO_ERROR;
	}


	virtual DWORD onSessionChange(DWORD dwEvent, PWTSSESSION_NOTIFICATION pSession)
	{
		Log::LogEventToFileSync(LOGLEVEL::Info, LOGTYPE::Audit, __func__, "SessionChangeEvent");
		return NO_ERROR;
	}



	virtual DWORD onPowerEvent(DWORD dwEvent, POWERBROADCAST_SETTING* pSetting)
	{
		Log::LogEventToFileSync(LOGLEVEL::Info, LOGTYPE::Audit, __func__, "PowerEvent");
		return NO_ERROR;
	}

	virtual void onUnknownRequest(DWORD /*dwControl*/) throw()
	{
		Log::LogEventToFileSync(LOGLEVEL::Info, LOGTYPE::Audit, __func__, "UnknownRequestEvent");
	}

	void setServiceStatus(DWORD dwState) throw()
	{
		status_.dwCurrentState = dwState;
		if (dwState == SERVICE_START_PENDING)
			status_.dwControlsAccepted = 0;
		else
			status_.dwControlsAccepted |= SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;

		if (!isDebugMode())
			::SetServiceStatus(hServiceStatus_, &status_);
	}

	//Implementation
protected:
	static void WINAPI _serviceMain(DWORD dwArgc, LPWSTR* lpszArgv) throw()
	{
		return GetInstance()->serviceMain(dwArgc, lpszArgv);
	}
	static DWORD WINAPI _serviceControlHandlerEx(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext) throw()
	{
		return reinterpret_cast<TConsoleService*>(lpContext)->serviceControlHandlerEx(dwControl, dwEventType, lpEventData);
	}
	DWORD serviceControlHandlerEx(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData)
	{
		DWORD dwRet = NO_ERROR;

		switch (dwControl)
		{
		case SERVICE_CONTROL_STOP:
			onStop();
			break;
		case SERVICE_CONTROL_PAUSE:
			onPause();
			break;
		case SERVICE_CONTROL_CONTINUE:
			onContinue();
			break;
		case SERVICE_CONTROL_INTERROGATE:
			onInterrogate();
			break;
#if (_WIN32_WINNT >= 0x0600)
		case SERVICE_CONTROL_PRESHUTDOWN:
			dwRet = onPreShutdown((LPSERVICE_PRESHUTDOWN_INFO)lpEventData);
			break;
#endif
		case SERVICE_CONTROL_SHUTDOWN:
			onShutdown();
			break;
		case SERVICE_CONTROL_DEVICEEVENT:
			dwRet = onDeviceEvent(dwEventType, (PDEV_BROADCAST_HDR)lpEventData);
			break;
		case SERVICE_CONTROL_HARDWAREPROFILECHANGE:
			dwRet = onHardwareProfileChange(dwEventType);
			break;
#if (_WIN32_WINNT >= 0x0501)
		case SERVICE_CONTROL_SESSIONCHANGE:
			dwRet = onSessionChange(dwEventType, (PWTSSESSION_NOTIFICATION)lpEventData);
			break;
#endif
#if (_WIN32_WINNT >= 0x0502)
		case SERVICE_CONTROL_POWEREVENT:
			dwRet = onPowerEvent(dwEventType, (POWERBROADCAST_SETTING*)lpEventData);
			break;
#endif
		default:
			onUnknownRequest(dwControl);
		}
		return dwRet;
	}
	static BOOL WINAPI _consoleCtrlHandler(DWORD dwCtrlType)
	{
		return GetInstance()->consoleCtrlHandler(dwCtrlType);
	}
	BOOL consoleCtrlHandler(DWORD dwCtrlType)
	{
		if (dwCtrlType == CTRL_C_EVENT
			|| dwCtrlType == CTRL_BREAK_EVENT
			|| dwCtrlType == CTRL_SHUTDOWN_EVENT) {
			// thunk to the service's STOP control handler
			onStop();
			return TRUE;
		}
		return FALSE;
	}




protected:
	std::wstring serviceName_;
	std::wstring serviceDescription_;
	std::wstring serviceUser_;
	std::vector<std::wstring> dependentOnSvcs_;

	SERVICE_STATUS_HANDLE hServiceStatus_ = nullptr;	// status handle
	HANDLE hEventQuit_ = nullptr;		// event that signals service termination
	SERVICE_STATUS status_{};	// service's current status
	bool fConsoleMode = false;			// set to true if /debug was specified

	std::function<DWORD(HANDLE hQuit, bool bConsole)> appMain_;
	std::function<bool(const std::wstring & serviceUser)> preInstallStart_ = nullptr;
};


