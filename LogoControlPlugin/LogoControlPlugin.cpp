// LogoControlPlugin.cpp : Defines the exported functions for the DLL.
//

#include "pch.h"
#include "framework.h"
#include "LogoControlPlugin.h"
#include "Interfaces.h"
#include <atomic>
#include <string>
#include "../Common/atomic_lock.h"
#include "../LibLogging/LibLogging.h"

class LogoControlPlugin :public IPlayBoxOut
{
private:
	std::atomic<ULONG> refcount = 0;
	std::atomic <int32_t> inited = -1;

	std::atomic<bool> data_lock = false;
	std::string last_error;
public:
	// Inherited via IPlayBoxOut
	virtual HRESULT __stdcall Init(void** DriverData) override
	{
		Log::info(__func__, "");
		if (DriverData == nullptr) return E_FAIL;
		*DriverData = nullptr;//we don't need it
		//retunrs 0 if inited ;1 if inited and activated;-1 on error
		if (inited < 0) inited = 0;
		return inited;
	}
	virtual HRESULT _stdcall Done(void* DriverData) override
	{
		Log::info(__func__, "");
		inited = -1;
		return S_OK;
	}
	virtual HRESULT _stdcall GetParams(TDriverParams* Params) override
	{
		Log::info(__func__, "");
		if (Params == nullptr) return E_POINTER;
		Params->DriverFlags;


		std::string name = "Logo control;Q";
		memcpy(Params->DriverName, name.c_str(), name.size() + 1);//add zero

		Params->ShortCut = 'Q';
		return S_OK;
	}
	virtual HRESULT _stdcall Config(void* DriverData) override
	{
		Log::info(__func__, "");
		return E_NOTIMPL;
	}
	virtual HRESULT _stdcall Execute(const char* Command, void* DriverData) override
	{
		Log::info(__func__, "");
		return E_NOTIMPL;
	}
	virtual HRESULT _stdcall NewCommand(char* Command, void* DriverData) override
	{
		Log::info(__func__, "");
		return E_NOTIMPL;
	}
	virtual HRESULT _stdcall EditCommand(char* Command, void* DriverData) override
	{
		Log::info(__func__, "");
		return E_NOTIMPL;
	}
	virtual HRESULT _stdcall Activate(void* DriverData) override
	{
		inited++;
		Log::info(__func__, "");
		return S_OK;
	}
	virtual HRESULT _stdcall Deactivate(void* DriverData) override
	{
		inited--;
		Log::info(__func__, "");
		return S_OK;
	}
	virtual HRESULT _stdcall GetLastErrorString(char* Text, DWORD* pSz) override
	{
		Log::info(__func__, "");
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
	virtual HRESULT __stdcall QueryInterface(REFIID riid, void** ppvObject) override
	{
		Log::info(__func__, "");
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
	virtual ULONG __stdcall AddRef(void) override
	{
		Log::info(__func__, "");
		return ++refcount;
	}
	virtual ULONG __stdcall Release(void) override
	{
		Log::info(__func__, "");
		refcount--;
		if (refcount == 0)
		{
			delete this;
		}
		return 0;
	}
};


HRESULT PlugInFunc(IUnknown** obj)
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
	return E_NOTIMPL;
}
