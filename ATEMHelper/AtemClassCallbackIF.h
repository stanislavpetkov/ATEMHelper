#pragma once
#include "BMDSwitcherAPI_h.h"

struct AtemClassCallbackIF
{
	virtual void InputNotification(const BMDSwitcherInputId inputId, BMDSwitcherInputEventType eventType) = 0;
	virtual void AudioInputNotification(const BMDSwitcherInputId inputId, BMDSwitcherAudioInputEventType eventType) = 0;
	virtual void AudioLevelNotification(const BMDSwitcherInputId inputId, double left, double right, double peakLeft, double peakRight) = 0;
};