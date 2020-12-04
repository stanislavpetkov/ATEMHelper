#pragma once
#include "BMDSwitcherAPI_h.h"
#include <atlbase.h>
#include <mutex>
#include "AtemClassCallbackIF.h"
#include "StringUtils.h"

class InputMonitor :
	public IBMDSwitcherInputCallback,
	public IBMDSwitcherAudioInputCallback
{
public:
	InputMonitor(CComPtr<IBMDSwitcherInput>& input, AtemClassCallbackIF* callback);
	~InputMonitor();

	BMDSwitcherPortType GetPortType() const;
	BMDSwitcherInputId GetVideoId() const;
	BMDSwitcherAudioInputId GetAudioId() const;


	std::string GetLongName() const;
	std::string GetShortName() const;

	bool isProgramTally();
	bool isPreviewTally();
	
	void SetAudioInputMonitorSolo(CComPtr<IBMDSwitcherAudioMonitorOutput>& monitorOutput);
	void SetAudioInput(CComPtr<IBMDSwitcherAudioInput>& input);

private:
	mutable std::recursive_mutex mtx;
	CComPtr<IBMDSwitcherInput> mInput;
	AtemClassCallbackIF* callback_;
	std::atomic<LONG> mRefCount = 0;
	BMDSwitcherInputId inputId = 0;
	BMDSwitcherAudioInputId audioInputId = 0;
	std::string inputNameLong;
	std::string inputNameShort;
	bool previewTally = false;
	bool programTally = false;
	BMDSwitcherPortType portType{};
	CComPtr<IBMDSwitcherAudioInput> audioInput = nullptr;

	//IUnknnown
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID* ppv);
	ULONG STDMETHODCALLTYPE AddRef(void);
	ULONG STDMETHODCALLTYPE Release(void);

	// IBMDSwitcherInputCallback interface
	virtual HRESULT STDMETHODCALLTYPE Notify(BMDSwitcherInputEventType eventType) override;




	// Inherited via IBMDSwitcherAudioInputCallback
	virtual HRESULT __stdcall Notify(BMDSwitcherAudioInputEventType eventType) override;
	virtual HRESULT __stdcall LevelNotification(double left, double right, double peakLeft, double peakRight) override;
	void SetPreviewTally(bool value);
	void SetProgramTally(bool value);
	void PopulateNames();
};

