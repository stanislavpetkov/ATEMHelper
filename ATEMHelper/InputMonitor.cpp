#include "InputMonitor.h"

//#define RESET_AUDIO_GAIN

InputMonitor::InputMonitor(CComPtr<IBMDSwitcherInput>& input, AtemClassCallbackIF* callback)
	: mInput(input), callback_(callback), mRefCount(1)
{
	mInput->GetInputId(&inputId);
	mInput->GetPortType(&portType);
	PopulateNames();


	BOOL isTally = FALSE;
	mInput->IsPreviewTallied(&isTally);
	SetPreviewTally(isTally != FALSE);
	isTally = FALSE;
	mInput->IsProgramTallied(&isTally);
	SetProgramTally(isTally != FALSE);


	mInput->AddCallback(this);


	printf("Input %I64u - name %s\n", inputId, inputNameLong.c_str());
}
void InputMonitor::SetPreviewTally(bool value)
{
	std::unique_lock lockit(mtx);
	previewTally = value;
	//do something
}


void InputMonitor::SetProgramTally(bool value)
{
	std::unique_lock lockit(mtx);
	programTally = value;
}

BMDSwitcherInputId InputMonitor::GetVideoId() const
{
	return inputId;
}


bool InputMonitor::isProgramTally()
{
	std::unique_lock lockit(mtx);
	return programTally;
}


bool InputMonitor::isPreviewTally()
{
	std::unique_lock lockit(mtx);
	return previewTally;
}

void InputMonitor::PopulateNames()
{
	std::unique_lock lockit(mtx);

	{
		CComBSTR name = nullptr;
		if (S_OK == mInput->GetLongName(&name))
		{

			std::wstring w(name);
			inputNameLong = wstring_to_utf8(w);
		}
		else
		{
			inputNameLong = "Unknown!!!";
		}
	}

	{
		CComBSTR name = nullptr;
		if (S_OK == mInput->GetShortName(&name))
		{

			std::wstring w(name);
			inputNameShort = wstring_to_utf8(w);
		}
		else
		{
			inputNameShort = "Unknown!!!";
		}
	}
}

std::string InputMonitor::GetLongName() const
{
	std::unique_lock lockit(mtx);
	return inputNameLong;
}


std::string InputMonitor::GetShortName() const
{
	std::unique_lock lockit(mtx);
	return inputNameShort;
}


BMDSwitcherPortType InputMonitor::GetPortType() const
{
	return portType;
}


InputMonitor::~InputMonitor()
{
	mInput->RemoveCallback(this);
	mInput = nullptr;

	if (audioInput != nullptr)
	{
		audioInput->RemoveCallback(this);
		audioInput = nullptr;
	}
}


HRESULT __stdcall InputMonitor::QueryInterface(REFIID iid, LPVOID* ppv)
{
	if (!ppv)
		return E_POINTER;

	if (IsEqualGUID(iid, IID_IBMDSwitcherInputCallback))
	{
		*ppv = static_cast<IBMDSwitcherInputCallback*>(this);
		AddRef();
		return S_OK;
	}

	if (IsEqualGUID(iid, IID_IBMDSwitcherAudioInputCallback))
	{
		*ppv = static_cast<IBMDSwitcherAudioInputCallback*>(this);
		AddRef();
		return S_OK;
	}

	if (IsEqualGUID(iid, IID_IUnknown))
	{
		*ppv = static_cast<IBMDSwitcherInputCallback*>(this);
		AddRef();
		return S_OK;
	}

	*ppv = NULL;
	return E_NOINTERFACE;
}
ULONG __stdcall InputMonitor::AddRef(void)
{
	return  ++mRefCount;
}


ULONG __stdcall InputMonitor::Release(void)
{
	return --mRefCount;
}


HRESULT __stdcall InputMonitor::Notify(BMDSwitcherInputEventType eventType)
{
	if ((eventType == bmdSwitcherInputEventTypeShortNameChanged)
		|| (eventType == bmdSwitcherInputEventTypeLongNameChanged)
		|| (eventType == bmdSwitcherInputEventTypeAreNamesDefaultChanged))
	{
		PopulateNames();
	}
	else
		if (eventType == bmdSwitcherInputEventTypeIsPreviewTalliedChanged)
		{
			BOOL isTally = FALSE;
			mInput->IsPreviewTallied(&isTally);
			SetPreviewTally(isTally != FALSE);
		}
		else
			if (eventType == bmdSwitcherInputEventTypeIsProgramTalliedChanged)
			{
				BOOL isTally = FALSE;
				mInput->IsProgramTallied(&isTally);
				SetProgramTally(isTally != FALSE);
#ifdef RESET_AUDIO_GAIN
				if (!isProgramTally())
				{
					if (audioInput)
					{
						audioInput->SetGain(0.0);
						audioInput->SetBalance(0.0);
					}
				}
#endif
			}

	printf("Input %I64u - name %s, Tally: pgm: %d, pvw: %d\n", inputId, inputNameLong.c_str(), programTally, previewTally);

	callback_->InputNotification(inputId, eventType);
	return S_OK;
}
BMDSwitcherAudioInputId InputMonitor::GetAudioId() const
{
	return audioInputId;
}

void InputMonitor::SetAudioInputMonitorSolo(CComPtr<IBMDSwitcherAudioMonitorOutput>& monitorOutput)
{
	monitorOutput->SetMonitorEnable(TRUE);
	monitorOutput->SetDim(FALSE);
	monitorOutput->SetDimLevel(-20.0);
	monitorOutput->SetMute(FALSE);
	monitorOutput->SetGain(0.0);
	monitorOutput->ResetLevelNotificationPeaks();
	monitorOutput->SetSolo(audioInputId != 0 ? TRUE : FALSE);

#ifdef RESET_AUDIO_GAIN
	if (!isProgramTally() && (audioInput))
	{
		audioInput->SetGain(0.0);
	}
#endif
	if (audioInputId != 0) monitorOutput->SetSoloInput(audioInputId);
}


void InputMonitor::SetAudioInput(CComPtr<IBMDSwitcherAudioInput>& input)
{
	std::unique_lock lockit(mtx);
	audioInput = input;
	audioInput->GetAudioId(&audioInputId);


	audioInput->SetMixOption(bmdSwitcherAudioMixOptionAudioFollowVideo);

#ifdef RESET_AUDIO_GAIN
	if (!isProgramTally())
	{
		audioInput->SetGain(0.0);
		audioInput->SetBalance(0.0);
	}
#endif

	
	audioInput->AddCallback(this);
}
HRESULT __stdcall InputMonitor::Notify(BMDSwitcherAudioInputEventType eventType)
{
	callback_->AudioInputNotification(inputId, eventType);
	return S_OK;
}

HRESULT __stdcall InputMonitor::LevelNotification(double left, double right, double peakLeft, double peakRight)
{
	callback_->AudioLevelNotification(inputId, left, right, peakLeft, peakRight);
	return S_OK;
}
