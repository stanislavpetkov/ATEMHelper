#pragma once
#include <cstdint>
#include <string>
#include "../Common/JSUtils.h"

struct BoardAPIModel
{
	std::string id;
	std::string model;
	std::string name;

	uint32_t numLogos;
	int32_t activeLogo;
	std::string activeLogoTxt;


	std::string cpuUsage;
	std::string cardTemperatureFront;
	std::string cardTemperatureRear;
	std::string ramUsage;
	std::string referenceStat;
	std::string inputAStat;
	std::string cardTime;

	int64_t last_update_time;
};


static void to_json(nlohmann::json& j, const BoardAPIModel& mdl)
{
	j = nlohmann::json::object();
	SetJSValue(j, mdl, id);
	SetJSValue(j, mdl, model);
	SetJSValue(j, mdl, name);
	SetJSValue(j, mdl, numLogos);
	SetJSValue(j, mdl, activeLogo);
	SetJSValue(j, mdl, activeLogoTxt);

	SetJSValue(j, mdl, cpuUsage);
	SetJSValue(j, mdl, cardTemperatureFront);
	SetJSValue(j, mdl, cardTemperatureRear);
	SetJSValue(j, mdl, ramUsage);
	SetJSValue(j, mdl, referenceStat);
	SetJSValue(j, mdl, inputAStat);
	SetJSValue(j, mdl, cardTime);
}


static void from_json(const nlohmann::json& j, BoardAPIModel& mdl)
{
	GetJSValue(j, mdl, id);
	GetJSValue(j, mdl, model);
	GetJSValue(j, mdl, name);
	GetJSValue(j, mdl, numLogos);
	GetJSValue(j, mdl, activeLogo);
	GetJSValue(j, mdl, activeLogoTxt);

	GetJSValue(j, mdl, cpuUsage);
	GetJSValue(j, mdl, cardTemperatureFront);
	GetJSValue(j, mdl, cardTemperatureRear);
	GetJSValue(j, mdl, ramUsage);
	GetJSValue(j, mdl, referenceStat);
	GetJSValue(j, mdl, inputAStat);
	GetJSValue(j, mdl, cardTime);
	mdl.last_update_time = 0;
}

struct BoardsAPIModel
{
	std::vector<BoardAPIModel> boards;
	int64_t timeStampMs;
};

static void to_json(nlohmann::json& j, const BoardsAPIModel& mdl)
{
	j = nlohmann::json::object();
	SetJSValue(j, mdl, boards);
	SetJSValue(j, mdl, timeStampMs);
}