#include "AtemClass.h"
#include <thread>
#include <atomic>
#include <memory>
#include <map>
#include <atlbase.h>
#include <mutex>
#include <concurrent_queue.h>
#include "AtemClassCallbackIF.h"
#include "BMDSwitcherAPI_h.h"
#include "InputMonitor.h"


struct SwitcherEvent
{
	BMDSwitcherEventType eventType;
	BMDSwitcherVideoMode coreVideoMode;
};


struct InputEvent
{
	BMDSwitcherInputId inputId;
	BMDSwitcherInputEventType eventType;
};

struct AtemClass::impl : public IBMDSwitcherCallback, public AtemClassCallbackIF
{
	std::atomic<unsigned long> mRefCount = 0;
	bool bRunning = true;
	std::thread atemCtrl;
	const std::string ip_;



	concurrency::concurrent_queue<SwitcherEvent> evt_queue;
	concurrency::concurrent_queue<InputEvent> input_evt_queue;

	void atemCtrlFn();
	impl(const std::string& ip) :ip_(ip) {
		atemCtrl = std::thread([this]() {return atemCtrlFn(); });
	};

	~impl()
	{
		bRunning = false;
		if (atemCtrl.joinable()) atemCtrl.join();
	}

	// Inherited via IBMDSwitcherCallback
	virtual HRESULT __stdcall QueryInterface(REFIID riid, void** ppvObject) override
	{
		if (!ppvObject)
			return E_POINTER;

		if (IsEqualGUID(riid, IID_IBMDSwitcherCallback))
		{
			*ppvObject = static_cast<IBMDSwitcherCallback*>(this);
			AddRef();
			return S_OK;
		}

		if (IsEqualGUID(riid, IID_IUnknown))
		{
			*ppvObject = static_cast<IUnknown*>(this);
			AddRef();
			return S_OK;
		}

		*ppvObject = NULL;
		return E_NOINTERFACE;
	}
	virtual ULONG __stdcall AddRef(void) override
	{
		return ++mRefCount;
	}
	virtual ULONG __stdcall Release(void) override
	{
		return --mRefCount;
	}
	virtual HRESULT __stdcall Notify(BMDSwitcherEventType eventType, BMDSwitcherVideoMode coreVideoMode) override
	{
		SwitcherEvent evt{};
		evt.coreVideoMode = coreVideoMode;
		evt.eventType = eventType;

		evt_queue.push(evt);
		return S_OK;
	}

	// Inherited via AtemClassCallbackIF
	virtual void InputNotification(const BMDSwitcherInputId inputId, BMDSwitcherInputEventType eventType) override
	{
		InputEvent evt{};
		evt.eventType = eventType;
		evt.inputId = inputId;

		input_evt_queue.push(evt);
	}

	virtual void AudioInputNotification(const BMDSwitcherInputId inputId, BMDSwitcherAudioInputEventType eventType) override
	{
		//ai
	}
	virtual void AudioLevelNotification(const BMDSwitcherInputId inputId, double left, double right, double peakLeft, double peakRight) override
	{
		//level
	}
};



void AtemClass::impl::atemCtrlFn()
{
	auto result = CoInitializeEx(NULL, NULL);
	if (FAILED(result))
	{
		fprintf(stderr, "Initialization of COM failed - result = %08x\n", result);
		return;
	}

	CComPtr<IBMDSwitcherDiscovery> switcherDiscovery = nullptr;
	result = switcherDiscovery.CoCreateInstance(CLSID_CBMDSwitcherDiscovery, NULL, CLSCTX_ALL);
	if (result != S_OK)
	{
		fprintf(stderr, "A Switcher Discovery instance could not be created.  The Switcher drivers may not be installed.\n");
		return;
	}

	CComBSTR ip(ip_.c_str());

	CComPtr<IBMDSwitcher> switcher = nullptr;


	std::map<BMDSwitcherInputId, std::unique_ptr<InputMonitor>> inputs;
	CComPtr<IBMDSwitcherAudioMonitorOutput> monitorOutput = nullptr;
	while (bRunning)
	{


		if (switcher == nullptr)
		{
			monitorOutput = nullptr;
			evt_queue.clear();
			input_evt_queue.clear();
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			BMDSwitcherConnectToFailure failure{};
			if (S_OK != switcherDiscovery->ConnectTo(ip, &switcher, &failure))
			{
				switcher = nullptr;
				continue;
			}
			//switcher connected
			if (S_OK != switcher->AddCallback(this))
			{
				switcher = nullptr;
				continue;
			}

			{
				CComPtr<IBMDSwitcherInputIterator> switcherInputIterator = nullptr;
				result = switcher->CreateIterator(IID_IBMDSwitcherInputIterator, (void**)&switcherInputIterator);
				if ((result != S_OK) || (switcherInputIterator == nullptr))
				{
					switcher->RemoveCallback(this);
					switcher = nullptr;
					continue;
				}

				while (true)
				{
					CComPtr<IBMDSwitcherInput> input = nullptr;
					if (S_OK != switcherInputIterator->Next(&input)) break;

					auto inpClass = std::make_unique<InputMonitor>(input, this);
					inputs[inpClass->GetVideoId()] = std::move(inpClass);
					input = nullptr;
				}
			}

			{
				CComQIPtr< IBMDSwitcherAudioMixer> audioMixer(switcher);
				if (audioMixer != nullptr)
				{


					CComPtr< IBMDSwitcherAudioMonitorOutputIterator> audioMIter = nullptr;
					result = audioMixer->CreateIterator(IID_IBMDSwitcherAudioMonitorOutputIterator, (void**)&audioMIter);
					if ((result != S_OK) || (audioMIter == nullptr))
					{
						switcher->RemoveCallback(this);
						switcher = nullptr;
						continue;
					}

					while (true)
					{
						CComPtr<IBMDSwitcherAudioMonitorOutput> output = nullptr;

						if (S_OK != audioMIter->Next(&output)) break;

						output->SetMonitorEnable(TRUE);

						output->SetDim(FALSE);
						output->SetDimLevel(-20.0);
						output->SetMute(FALSE);
						output->SetGain(0.0);
						output->SetSolo(FALSE);
						output->ResetLevelNotificationPeaks();

						monitorOutput = output;
						//output->AddCallback


						break;
					}
					audioMixer->SetAudioFollowVideoCrossfadeTransition(TRUE);


					//audioMixer->AddCallback


					CComPtr< IBMDSwitcherAudioInputIterator> audioIter = nullptr;
					result = audioMixer->CreateIterator(IID_IBMDSwitcherAudioInputIterator, (void**)&audioIter);
					if ((result != S_OK) || (audioIter == nullptr))
					{
						switcher->RemoveCallback(this);
						switcher = nullptr;
						continue;
					}

					while (true)
					{
						CComPtr<IBMDSwitcherAudioInput> input = nullptr;
						if (S_OK != audioIter->Next(&input)) break;
						BMDSwitcherAudioInputId audioId{};
						input->GetAudioId(&audioId);
						BMDSwitcherAudioInputType inType{};


						input->GetType(&inType);




						//Find the video input
						if (inType == bmdSwitcherAudioInputTypeEmbeddedWithVideo)
						{
							auto in = inputs.find(audioId);
							if (in != inputs.end())
							{
								if (in->second->GetPortType() == bmdSwitcherPortTypeExternal)
								{
									in->second->SetAudioInput(input);
									if (in->second->isPreviewTally())
									{
										if (monitorOutput)
										{
											in->second->SetAudioInputMonitorSolo(monitorOutput);
										}
									}

								}
							}
						}
						else if (inType == bmdSwitcherAudioInputTypeMediaPlayer)
						{
							auto mediaPlayerNo = audioId - 2000;
							size_t mpNo = 0;
							for (auto& in : inputs)
							{
								if (in.second->GetPortType() == bmdSwitcherPortTypeMediaPlayerFill)
								{
									mpNo++;
									if (mpNo == mediaPlayerNo)
									{
										in.second->SetAudioInput(input);

										if (in.second->isPreviewTally())
										{
											if (monitorOutput)
											{
												in.second->SetAudioInputMonitorSolo(monitorOutput);
											}
										}
										break;
									}
								}
							}
						}
						else if (inType == bmdSwitcherAudioInputTypeAudioIn)
						{
							input->SetMixOption(bmdSwitcherAudioMixOptionOff);
							input->SetGain(-INFINITY);
						};
						input = nullptr;
					}
				}
			}
			printf("Connected\n");
			continue;
		}
		{
			SwitcherEvent evt;
			while (evt_queue.try_pop(evt))
			{
				//do something
				if (evt.eventType == BMDSwitcherEventType::bmdSwitcherEventTypeDisconnected)
				{
					printf("Disconnected\n");
					switcher->RemoveCallback(this);
					inputs.clear();

					switcher = nullptr;
					break;
				}
			}
		}
		{
			InputEvent evt;
			while (input_evt_queue.try_pop(evt))
			{
				auto input = inputs.find(evt.inputId);
				if (input == inputs.end()) continue;

				if (evt.eventType == bmdSwitcherInputEventTypeIsPreviewTalliedChanged)
				{
					if (input->second->isPreviewTally())
					{
						if (monitorOutput)
						{
							input->second->SetAudioInputMonitorSolo(monitorOutput);
						}
					}
				}

			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	if (switcher)
	{
		switcher->RemoveCallback(this);
		switcher = nullptr;
	}
	CoUninitialize();
}


AtemClass::AtemClass(const std::string& ip) :
	impl_(std::make_unique<impl>(ip))
{
}

AtemClass::~AtemClass()
{
	impl_ = nullptr;
}