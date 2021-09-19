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
};


void to_json(nlohmann::json& j, const BoardAPIModel& mdl)
{
	j = nlohmann::json::object();
	SetJSValue(j, mdl, id);
	SetJSValue(j, mdl, model);
	SetJSValue(j, mdl, name);
	SetJSValue(j, mdl, numLogos);
	SetJSValue(j, mdl, activeLogo);
}