// LogoControlPlugin.cpp : Defines the exported functions for the DLL.
//

#include "pch.h"
#include "framework.h"
#include "LogoControlPlugin.h"
#include "Interfaces.h"
#include <atomic>
#include <concurrent_queue.h>
#include <string>
#include "../Common/atomic_lock.h"
#include "../Common/sys_utils.h"
#include "../Common/ThirdParty/nlohmann_json/src/json.hpp"
#include "../LibLogging/LibLogging.h"
#include "resource.h"
#include <fstream>
#include "../LogoControl/LogoSel.h"
#include "../LogoControl/BoardAPIModel.h"
#include <ws2tcpip.h>
#include "../Common/Http Client/HTTPRequest.hpp"

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "Ws2_32.lib")

#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

constexpr HRESULT E_CANCEL_ERR = HRESULT(-1);

#pragma pack( push,1)
struct InitParams
{
	uint8_t airboxInstance;
	uint32_t wibuMask;
	HANDLE AppHandle;
};
#pragma pack(pop)

struct ExecutionCommand
{
	LogoSel logo = LogoSel::logoOff;
};


struct PluginCommand
{
	LogoSel logo = LogoSel::logoOff;
	int32_t offset_ms = 0;


	WNDPROC  prevCall = 0;
};

struct PluginSettings
{
	std::string server_ip;
	std::string logo_id;
};

void to_json(nlohmann::json& js, const PluginSettings& model)
{
	js = nlohmann::json::object();
	js["server_ip"] = model.server_ip;
	js["logo_id"] = model.logo_id;
}

void from_json(const nlohmann::json& js, PluginSettings& model)
{
	if (!js.is_object()) {
		model = {};
		return;
	}
	model.server_ip = "127.0.0.1";
	model.logo_id = "LOGO S/N";

	auto svrip = js.find("server_ip");
	if (svrip->is_string())	model.server_ip = svrip->get<std::string>();

	auto logoid = js.find("logo_id");
	if (logoid->is_string())	model.logo_id = logoid->get<std::string>();
}

class LogoControlPlugin :public IPlayBoxOut
{
public:
	HWND appHandle = nullptr;
private:
	std::atomic<ULONG> refcount = 0;
	std::atomic <int32_t> inited = -1;

	
	std::atomic<bool> data_lock = false;
	PluginSettings plg_settings{};
	std::string last_error;

	bool bRunning = true;

	concurrency::concurrent_queue<ExecutionCommand> cmdQueue;
	HANDLE eventCheckForData = CreateEvent(nullptr, false, false, nullptr);
	std::thread execution_thread;

	void ExecutionThreadFn();


	static INT_PTR CALLBACK DialogProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
	static INT_PTR CALLBACK DialogPlgCfgProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
	static LRESULT CALLBACK SignedIntegerSubclassProc(HWND hWnd, UINT uMessage, WPARAM wParam, LPARAM lParam);

	bool DoEditDialog(PluginCommand& settings);
	bool DoPluginCfgDialog(PluginSettings& settings);


	static PluginSettings LoadSettings()
	{
		pbn::fs::Path p = pbn::OS::GetProgramDataFolder();
		p.append("LogoControlPlugin");

		pbn::fs::create_dirs(p);
		p.append("config.json");
		if (!pbn::fs::file_exist(p))
		{
			PluginSettings set;
			set.logo_id = "LOGO S/N";
			set.server_ip = "127.0.0.1";
			SaveSettings(set);
		}

		std::ifstream ifs(p.wstring());
		if (!ifs.is_open()) return {};

		try
		{
			nlohmann::json j = nlohmann::json::parse(ifs);

			return j;
		}
		catch (const std::exception& e)
		{
			Log::error(__func__, "Config file ::: {}", e.what());
		}
		catch (...)
		{
			Log::error(__func__, "Wrong config file format");

		}
		return {};
	}

	static void SaveSettings(const PluginSettings& settings)
	{
		pbn::fs::Path p = pbn::OS::GetProgramDataFolder();
		p.append("LogoControlPlugin");

		pbn::fs::create_dirs(p);
		p.append("config.json");

		std::ofstream ofs(p.wstring());
		if (!ofs.is_open()) {
			Log::error(__func__, "Can not save settings");
			return;
		}

		nlohmann::json j = settings;
		std::string s = j.dump();
		ofs.write(s.data(), s.size());
		ofs.flush();
		ofs.close();
	}


	void SendCommand(ExecutionCommand&& cmd)
	{
		cmdQueue.push(std::move(cmd));
		SetEvent(eventCheckForData);
	}
public:

	LogoControlPlugin()
	{
		execution_thread = std::thread([this]() {return ExecutionThreadFn(); });
		inited = 1;//Always active
	}

	~LogoControlPlugin()
	{
		bRunning = false;
		SetEvent(eventCheckForData);
		if (execution_thread.joinable()) execution_thread.join();
		CloseHandle(eventCheckForData);
	}


	// Inherited via IPlayBoxOut
	virtual HRESULT _stdcall Init(void** DriverData) override
	{
		Log::info(__func__, "Init");
		if (DriverData == nullptr) return E_FAIL;
		

		InitParams* params = (InitParams*)(*DriverData);

		{
			atomic_lock lock(data_lock);
			plg_settings = LoadSettings();
		}


		//retunrs 0 if inited ;1 if inited and activated;-1 on error
		if (inited < 0) inited = 0;
		return inited;
	}
	virtual HRESULT _stdcall Done(void* DriverData) override
	{


		bRunning = false;
		SetEvent(eventCheckForData);
		if (execution_thread.joinable()) execution_thread.join();


		inited = -1;
		return S_OK;
	}
	virtual HRESULT _stdcall GetParams(TDriverParams* Params) override
	{
		Log::info(__func__, "GetParams");
		if (Params == nullptr) return E_POINTER;
		Params->DriverFlags;


		std::string name = "Logo control;Q";
		memcpy(Params->DriverName, name.c_str(), name.size() + 1);//add zero

		Params->ShortCut = 'Q';
		return S_OK;
	}
	virtual HRESULT _stdcall Config(void* DriverData) override
	{
		Log::info(__func__, "Config");

		PluginSettings pls;

		{
			atomic_lock lock(data_lock);
			pls = plg_settings;
		}
		if (!DoPluginCfgDialog(pls))
		{
			last_error = "Edit canceled";
			Log::info(__func__, "Nothing new... cancel");
			return E_CANCEL_ERR;
		}
		{
			atomic_lock lock(data_lock);
			plg_settings = pls;
		}

		SaveSettings(pls);
		return S_OK;
	}
	virtual HRESULT _stdcall Execute(const char* Command, void* DriverData) override
	{
		
		auto cmd = deserialize(Command);

		last_error = fmt::format("Executing::: Logo {}, offset {}", cmd.logo, cmd.offset_ms);
		Log::info("", last_error);
		ExecutionCommand ec;

		ec.logo = cmd.logo;
		SendCommand(std::move(ec));


		return S_OK;
	}



	std::string serialize(const PluginCommand& set)
	{
		switch (set.logo)
		{
		case LogoSel::logoOff: return fmt::format("LOGO OFF;{};{};0;0", set.offset_ms, set.logo);
		case LogoSel::logo1On: return fmt::format("LOGO 1 ON;{};{};0;0", set.offset_ms, set.logo);
		case LogoSel::logo2On: return fmt::format("LOGO 2 ON;{};{};0;0", set.offset_ms, set.logo);
		default:
			break;
		}
		return fmt::format("LOGO ??;{};{};0;0", set.offset_ms, LogoSel::logoOff);
	}


	PluginCommand deserialize(const std::string& cmd)
	{
		auto splres = pbn::split(cmd, ';');
		PluginCommand res;
		if (splres.size() != 5) return res;

		try
		{


			res.offset_ms = std::stol(splres[1]);
			auto logoidx = std::stoul(splres[2]);
			if (logoidx >= static_cast<uint32_t>(LogoSel::logoMax)) return res;
			res.logo = static_cast<LogoSel>(logoidx);
		}
		catch (const std::exception&)
		{
			return {};
		}

		return res;
	}

	virtual HRESULT _stdcall NewCommand(char** Command, void* DriverData) override
	{
		Log::info(__func__, "NewCommand");
		PluginCommand set{};
		if (!DoEditDialog(set))
		{
			last_error = "Edit canceled";
			Log::info(__func__, "Nothing new... cancel");
			return E_CANCEL_ERR;
		}

		//do something with the settings
		auto res = serialize(set);
		memcpy(*Command, res.data(), res.size() + 1); //give it a zero term
		return S_OK;
	}
	virtual HRESULT _stdcall EditCommand(char** Command, void* DriverData) override
	{
		Log::info(__func__, "EditCommand");

		PluginCommand set = deserialize(*Command);
		if (!DoEditDialog(set))
		{
			last_error = "Edit canceled";
			Log::info(__func__, "Nothing new... cancel");
			return E_CANCEL_ERR;
		}
		auto res = serialize(set);
		memcpy(*Command, res.data(), res.size() + 1); //give it a zero term
		return S_OK;
	}
	virtual HRESULT _stdcall Activate(void* DriverData) override
	{
		inited++;
		Log::info(__func__, "Activate");
		return S_OK;
	}
	virtual HRESULT _stdcall Deactivate(void* DriverData) override
	{
		inited--;
		Log::info(__func__, "Deactivate");
		return S_OK;
	}
	virtual HRESULT _stdcall GetLastErrorString(char* Text, DWORD* pSz) override
	{
		Log::info(__func__, "GetLastErrorString");
		if ((Text == nullptr) || (pSz == nullptr)) return E_POINTER;
		atomic_lock lovk(data_lock);
		if (last_error.empty())
		{
			*Text = '\0';
			*pSz = 0;
			return S_OK;
		}

		DWORD sz = std::min(*pSz, static_cast<DWORD>(last_error.size() + 1));

		memcpy(Text, last_error.c_str(), sz); //copy with zero
		*pSz = sz - 1;

		last_error.clear();
		return S_OK;
	}
	virtual HRESULT _stdcall About() override
	{
		Log::info(__func__, "");
		return E_NOTIMPL;
	}

	// Inherited via IPlayBoxOut
	virtual HRESULT _stdcall QueryInterface(REFIID riid, void** ppvObject) override
	{
		Log::info(__func__, "QueryInterface");
		if (ppvObject == nullptr) return E_POINTER;


		/* Do we have this interface */

		if (riid == __uuidof(IPlayBoxOut) || riid == IID_IUnknown) {
			AddRef();
			*ppvObject = this;
			return S_OK;
		}
		else {
			*ppvObject = nullptr;
			return E_NOINTERFACE;
		}
	}
	virtual ULONG _stdcall AddRef(void) override
	{

		ULONG ret = ++refcount;
		Log::info(__func__, "AddRef {}", ret);
		return ret;
	}
	virtual ULONG _stdcall Release(void) override
	{
		auto last = --refcount;
		Log::info(__func__, "Release {}", refcount.load());

		if (refcount == 0)
		{
			delete this;
		}
		return last;
	}
};


HRESULT _stdcall PlugInFunc(IUnknown** obj)
{
	Log::SetAsyncLogging(true);
	Log::SetODSLogging(true);
	Log::SetConsoleLogging(false);
	Log::SetFileLogging(true);


	if (obj == nullptr) return E_POINTER;

	auto classPtr = new LogoControlPlugin();
	if (const auto res = classPtr->QueryInterface(__uuidof(IUnknown), (void**)obj); res != S_OK)
	{
		delete classPtr;
		return E_FAIL;
	}
	return S_OK;
}


bool is_ipv4_ok(const std::string& ip)
{
	if (ip.empty()) return false;
	IN_ADDR ipv4{};
	if (1 != inet_pton(AF_INET, ip.c_str(), &ipv4)) return false;

	auto multicast = (ipv4.S_un.S_un_b.s_b1 & 0xE0) == 0xE0;
	auto fullbcast = (ipv4.S_un.S_un_b.s_b1 == 0xFF) &&
		(ipv4.S_un.S_un_b.s_b2 == 0xFF) &&
		(ipv4.S_un.S_un_b.s_b3 == 0xFF) &&
		(ipv4.S_un.S_un_b.s_b4 == 0xFF);

	return (!multicast) && (!fullbcast);
}


int ExecuteGet(const std::string& url, std::vector<uint8_t>& responseBody)
{
	http::Request req(url);
	try
	{
		auto response = req.send("GET", "", {}, std::chrono::milliseconds(500));


		responseBody.swap(response.body);
		return response.status;
	}
	catch (...)
	{
		return 404;
	}

}

std::string make_url_execute(const std::string& ip, const std::string& logoId, LogoSel logo)
{
	return fmt::format("http://{}:9012/api/boards/{}/logo?logo={}", ip, logoId, logo);
}

std::string make_url_get_boards(const std::string& ip)
{
	return fmt::format("http://{}:9012/api/boards", ip);
}

void LogoControlPlugin::ExecutionThreadFn()
{
	ExecutionCommand cmd;
	std::vector<BoardAPIModel> boards;
	PluginSettings local;
	std::vector<uint8_t> body_buffer;

	timeBeginPeriod(1);

	while (bRunning)
	{

		{
			atomic_lock lock(data_lock);
			local = plg_settings;
		}

		if (cmdQueue.try_pop(cmd))
		{

			if (!is_ipv4_ok(local.server_ip))
			{
				Log::error(__func__, "Server IP Invalid... '{}'", local.server_ip);
				continue;
			}

			if (local.logo_id.empty())
			{
				Log::error(__func__, "LogoID empty....");
				continue;
			}

			auto has_board = std::any_of(boards.begin(), boards.end(), [boards, local](const BoardAPIModel& brd) {
				return brd.id == local.logo_id;
				});

			if (!has_board)
			{
				Log::warn(__func__, "Logo identifier not found on server. Will try anyway");
			}

			{
				Log::PerformanceLog pl("Execution Time");
				auto url = make_url_execute(local.server_ip, local.logo_id, cmd.logo);

				auto res = ExecuteGet(url, body_buffer);

				//do execute
				Log::info(__func__, "Execute:: IP: {}, LID: {}, LogoNo: {}. Result: {}", local.server_ip, local.logo_id, cmd.logo, res);
			}


			continue;
		}

		if (is_ipv4_ok(local.server_ip))
		{
			//Check server status
			auto url = make_url_get_boards(local.server_ip);
			auto res = ExecuteGet(url, body_buffer);

			if (res == 200)
			{
				try
				{
					nlohmann::json j = nlohmann::json::parse(body_buffer);
					if (j.is_array())
					{
						boards.clear();
						for (auto& elm : j)
						{
							boards.push_back(elm);
						}
					}
				}
				catch (...)
				{
					boards.clear();
				}
			}
			else
				Log::warn(__func__, "Get Boards Error Code {}", res);
		}
		WaitForSingleObject(eventCheckForData, 2000);
	}

	timeEndPeriod(1);
	Log::info(__func__, "Comms thread done");
}

bool IsUnicodeDigit(wchar_t ch)
{
	return ((ch >= L'0') && (ch <= L'9'));
}
INT_PTR CALLBACK LogoControlPlugin::DialogProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{


	
	if (message == WM_INITDIALOG)
	{
		SetWindowLongPtr(hwnd, DWLP_USER, lparam);

		//initialize the brush


		if (lparam == 0) return 1;
	}

	auto settings = (PluginCommand*)(GetWindowLongW(hwnd, DWLP_USER));
	if (settings == nullptr) {
		return (LRESULT)1;
	}







	switch (message)
	{
	
	case WM_INITDIALOG:
	{
		//Do something for initialization if needed

		int radio = IDC_RADIO1;
		switch (settings->logo)
		{
		case LogoSel::logoOff: radio = IDC_RADIO1; break;
		case LogoSel::logo1On:radio = IDC_RADIO2; break;
		case LogoSel::logo2On:radio = IDC_RADIO3; break;
		default:
			break;
		}

		CheckRadioButton(
			hwnd,
			IDC_RADIO1,
			IDC_RADIO3,
			radio
		);


		SetDlgItemInt(hwnd, IDC_OFFSET, settings->offset_ms, TRUE);

		auto offsetHwnd = GetDlgItem(hwnd, IDC_OFFSET);

		settings->prevCall = (WNDPROC)SetWindowLong(offsetHwnd, GWL_WNDPROC, (LPARAM)SignedIntegerSubclassProc);

		auto j = SetWindowLongPtr(offsetHwnd, GWLP_USERDATA, LPARAM(settings));

		break;
	}





	case WM_STYLECHANGING:
		if (wparam == GWL_EXSTYLE) {
			LPSTYLESTRUCT lpss = (LPSTYLESTRUCT)lparam;
			lpss->styleNew |= WS_EX_CONTROLPARENT;
			return LRESULT(0);
		}
		break;

	case WM_CLOSE:
	{

		PostQuitMessage(wparam);
		DestroyWindow(hwnd);
		return  LRESULT(0);
	}


	case WM_COMMAND:

		auto control = LOWORD(wparam);
		auto controlmsg = HIWORD(wparam);
		if (control == IDOK)
		{
			PostMessage(hwnd, WM_CLOSE, IDOK, 0);
			return  LRESULT(0);
		}


		if (control == IDCANCEL)
		{
			PostMessage(hwnd, WM_CLOSE, IDCANCEL, 0);
			return  LRESULT(0);
		}

		if (control == IDC_OFFSET)
		{


			BOOL isTranslated = FALSE;
			settings->offset_ms = GetDlgItemInt(hwnd, control, &isTranslated, true/*signed*/);
			return  LRESULT(0);
		}

		if ((control == IDC_RADIO1) ||
			(control == IDC_RADIO2) ||
			(control == IDC_RADIO3))
		{
			auto checked = IsDlgButtonChecked(
				hwnd,
				control
			) == BST_CHECKED;

			if (!checked)  return  LRESULT(0);


			switch (control)
			{
			case IDC_RADIO1: settings->logo = LogoSel::logoOff; break;
			case IDC_RADIO2: settings->logo = LogoSel::logo1On; break;
			case IDC_RADIO3: settings->logo = LogoSel::logo2On; break;
			default:
				break;
			}



		}


	}
	return DefWindowProcW(hwnd, message, wparam, lparam);
}

INT_PTR CALLBACK LogoControlPlugin::DialogPlgCfgProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{


	if (message == WM_INITDIALOG)
	{
		SetWindowLongPtr(hwnd, DWLP_USER, lparam);

		//initialize the brush


		if (lparam == 0) return 1;
	}

	auto settings = (PluginSettings*)(GetWindowLongW(hwnd, DWLP_USER));
	if (settings == nullptr) {
		return (LRESULT)1;
	}












	switch (message)
	{
	case WM_INITDIALOG:
	{
		//Do something for initialization if needed
		std::wstring wIP = pbn::to_wstring(settings->server_ip);
		std::wstring wID = pbn::to_wstring(settings->logo_id);

		SetDlgItemTextW(hwnd, IDC_SVRIP, wIP.c_str());
		SetDlgItemTextW(hwnd, IDC_LOGOID, wID.c_str());



		break;
	}





	case WM_STYLECHANGING:
		if (wparam == GWL_EXSTYLE) {
			LPSTYLESTRUCT lpss = (LPSTYLESTRUCT)lparam;
			lpss->styleNew |= WS_EX_CONTROLPARENT;
			return LRESULT(0);
		}
		break;

	case WM_CLOSE:
	{

		PostQuitMessage(wparam);
		DestroyWindow(hwnd);
		return  LRESULT(0);
	}


	case WM_COMMAND:

		auto control = LOWORD(wparam);
		auto controlmsg = HIWORD(wparam);
		if (control == IDOK)
		{
			PostMessage(hwnd, WM_CLOSE, IDOK, 0);
			return  LRESULT(0);
		}


		if (control == IDCANCEL)
		{
			PostMessage(hwnd, WM_CLOSE, IDCANCEL, 0);
			return  LRESULT(0);
		}

		if (control == IDC_SVRIP)
		{
			std::wstring w;
			w.resize(64);
			auto strsz = GetDlgItemText(hwnd, control, (LPWSTR)w.c_str(), w.size());
			w.resize(strsz);

			settings->server_ip = pbn::to_utf8(w);


			return  LRESULT(0);
		}


		if (control == IDC_LOGOID)
		{
			std::wstring w;
			w.resize(64);

			auto strsz = GetDlgItemText(hwnd, control, (LPWSTR)w.c_str(), w.size());
			w.resize(strsz);
			settings->logo_id = pbn::to_utf8(w);

			return  LRESULT(0);
		}
	}
	return DefWindowProcW(hwnd, message, wparam, lparam);
}

LRESULT LogoControlPlugin::SignedIntegerSubclassProc(HWND hWnd, UINT uMessage, WPARAM wParam, LPARAM lParam)
{

	auto settings = (PluginCommand*)(GetWindowLongW(hWnd, GWLP_USERDATA));
	if (settings == nullptr) {
		return (LRESULT)1;
	}

	constexpr auto  ACCEPTED_CHARS = L"0123456789+-\b";


	switch (uMessage) {
	case WM_CHAR:
		auto c = static_cast<wchar_t>(wParam);
		if (wcschr(ACCEPTED_CHARS, c) == nullptr)
			return(0);


		if ((c == L'+') || (c == L'-'))
		{
			std::wstring w;
			SetLastError(0);
			auto len = GetWindowTextLengthW(hWnd);

			size_t startP = 0,
				endP = 0;


			SendMessageW(hWnd, EM_GETSEL, WPARAM(&startP), LPARAM(&endP));



			w.resize(len + 1); //include zero term
			auto res = GetWindowTextW(hWnd, w.data(), w.size());
			w.resize(res);
			if (len == res)
			{
				if (startP != endP)
				{
					w.erase(w.begin() + startP, w.begin() + endP);
				}


				if ((w.size() == 0) && (c == L'-'))
					return CallWindowProcW(settings->prevCall, hWnd, uMessage, wParam, lParam);

				if ((w[0] == '-') && (c == L'-')) return 0;
				if ((w[0] == '-') && (c == L'+'))
				{
					w.erase(w.begin());
					if (startP > 0) startP--;
				}
				else
					if ((w[0] == '+') && (c == L'-'))
					{
						w.erase(w.begin());
						w = L"-" + w;
						startP = std::min(startP + 1, w.size() - 1);
					}
					else
						if (c == L'-') {
							w = L"-" + w;
							startP = std::min(startP + 1, w.size() - 1);
						}

				SetWindowTextW(hWnd, w.data());


				SendMessage(hWnd, EM_SETSEL, (WPARAM)startP, (LPARAM)startP);
				/* "Replace" the selection (the selection is actually targeting
					nothing and just sits at the end of the text in the box)
					with the passed in TCHAR* from the button control that
					sent the WM_APPEND_EDIT message */
					//SendMessage(hWnd, EM_REPLACESEL, 0, lParam);
			}
			return(0); // dont send them to default
		}
		break;
	}

	return CallWindowProcW(settings->prevCall, hWnd, uMessage, wParam, lParam);

}


bool LogoControlPlugin::DoEditDialog(PluginCommand& settings)
{

	HINSTANCE hi = reinterpret_cast<HINSTANCE>(pbn::OS::GetCurrentModuleInstance());
	HWND dialog{};
	PluginCommand localSettings = settings;

	auto window = GetActiveWindow(); 
	

	dialog = CreateDialogParamW(hi, MAKEINTRESOURCE(IDD_EDITDIALOG), window, DialogProc, LPARAM(&localSettings)); //LPARAM is same 32bit in 32bit app and 64 in 64


	
	if (dialog == 0)
	{
		MessageBox(nullptr, L"Can not crete dialog", L"Error", MB_ICONERROR);
		return false;
	}

	


	ShowWindow(dialog, SW_NORMAL);
	UpdateWindow(dialog);

	MSG msg{};
	while (GetMessageW(&msg, 0, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	if (msg.wParam != IDOK) return false;

	settings = localSettings;


	return true;
}



bool LogoControlPlugin::DoPluginCfgDialog(PluginSettings& settings)
{

	HINSTANCE hi = reinterpret_cast<HINSTANCE>(pbn::OS::GetCurrentModuleInstance());
	HWND dialog{};
	PluginSettings localSettings = settings;


	auto window = GetActiveWindow();

	dialog = CreateDialogParamW(hi, MAKEINTRESOURCE(IDD_PLUGCFG_DLG), window, DialogPlgCfgProc, LPARAM(&localSettings)); //LPARAM is same 32bit in 32bit app and 64 in 64

	if (dialog == 0)
	{
		MessageBox(nullptr, L"Can not crete dialog", L"Error", MB_ICONERROR);
		return false;
	}




	ShowWindow(dialog, SW_NORMAL);
	UpdateWindow(dialog);

	MSG msg{};
	while (GetMessageW(&msg, 0, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	if (msg.wParam != IDOK) return false;

	settings = localSettings;


	return true;
}