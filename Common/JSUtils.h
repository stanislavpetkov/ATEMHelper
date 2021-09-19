#pragma once
#include "ThirdParty/nlohmann_json/src/json.hpp"

#include <fmt/format.h>
#include <stdexcept>

#define SetJSNameValue(js , name, elm ) js[name] = elm

#define SetJSValue(js , struct_, elm ) js[#elm] = struct_.elm
#define SetJSPtrValue(js , struct_, elm ) js[#elm] = struct_->elm
#define __TypeName__(elm) #elm

template <typename T>
void GetJSValue_(const nlohmann::json & js, const char* valName, const char* structName, T & val)
{
	if (js.find(valName) == js.end()) throw std::exception(fmt::format("{} Property {} is missing", structName, valName).c_str()); 
	val = js[valName].get<T>();
}

template <typename T>
void GetJSValueByName(const nlohmann::json& js, const char* valName, const char* structName, T& val)
{
	if (js.find(valName) == js.end()) throw std::exception(fmt::format("{} Property {} is missing", structName, valName).c_str());
	val = js[valName].get<T>();
}

#define GetJSValue(js, struct_, elm) {GetJSValue_(js, #elm, typeid(struct_).name(), struct_.elm);}
#define GetJSPtrValue(js, struct_, elm) {GetJSValue_(js, #elm, typeid(struct_).name(), struct_->elm);}