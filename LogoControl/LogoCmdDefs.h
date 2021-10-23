#pragma once
#include <string>
#include <cstdint>
#include <map>
#include <vector>
#include "../Common/ThirdParty/nlohmann_json/src/json.hpp"

struct setResult
{
	uint32_t index;
	uint32_t oid;
	std::string setRes;
	nlohmann::json value;
};


static void from_json(const nlohmann::json& j, setResult& mdl)
{
	if (!j.is_object()) throw std::exception("Invalid value");
	mdl.index = j["index"].get<uint32_t>();
	mdl.oid = j["oid"].get<uint32_t>();
	mdl.setRes = j["setResult"].get<std::string>();
	mdl.value = j["value"];
}

struct update
{
	std::string oid;
	nlohmann::json data_value;
};

static void from_json(const nlohmann::json& j, update& mdl)
{
	if (!j.is_object()) throw std::exception("Invalid value");

	auto data_value = j.find("data_value");
	auto oid = j.find("oid");

	if (oid->is_string())
	{
		mdl.oid = oid->get<std::string>();
	}
	else mdl.oid = std::to_string(oid->get<int64_t>());




	mdl.data_value = *data_value;

}

struct pupdates
{
	std::vector<update> oids;
	std::string uuid;
};

static void from_json(const nlohmann::json& j, pupdates& mdl)
{
	if (!j.is_object()) throw std::exception("Invalid value");

	auto uuid = j.find("uuid");
	auto oids = j.find("oids");
	mdl.uuid = uuid->get<std::string>();

	mdl.oids.clear();
	for (const auto& oid : *oids)
	{
		mdl.oids.push_back(oid);
	}
}
struct updates_packet
{
	pupdates updates;
};

static void from_json(const nlohmann::json& j, updates_packet& mdl)
{
	if (!j.is_object()) throw std::exception("Invalid value");

	auto updates = j.find("updates");

	mdl.updates = updates->get<pupdates>();

}


struct pdata_updates
{
	std::map<std::string, nlohmann::json> oids;
	std::string uuid;
};

static void from_json(const nlohmann::json& j, pdata_updates& mdl)
{
	if (!j.is_object()) throw std::exception("Invalid value");

	auto uuid = j.find("uuid");
	auto oids = j.find("oids");
	mdl.uuid = uuid->get<std::string>();


	mdl.oids = oids->get<std::map<std::string, nlohmann::json>>();

}

struct data_updates_packet
{
	pdata_updates updates;
};

static void from_json(const nlohmann::json& j, data_updates_packet& mdl)
{
	if (!j.is_object()) throw std::exception("Invalid value");

	auto updates = j.find("data_updates");

	mdl.updates = updates->get<pdata_updates>();

}



struct setOid
{
	uint32_t oid;
	nlohmann::json value;
	uint32_t index = 0;
	std::string uuid;
};

static void to_json(nlohmann::json& j, const setOid& mdl)
{
	j = nlohmann::json::object();
	j["setOid"]["oid"] = mdl.oid;
	j["setOid"]["value"] = mdl.value;
	j["setOid"]["index"] = mdl.index;
	j["setOid"]["uuid"] = mdl.uuid;
}



struct setOids
{
	std::vector<setOid> oids;
	std::string uuid;

	/*void AddValue(const std::string& Oid, const std::string& Value, const std::string& Index)
	{
		setOid& o = oids.emplace_back();
		o.index = std::stoul(Index);
		o.oid = std::stoul( Oid);
		o.uuid = uuid;
		o.value = Value;
	}*/

	void AddValue(const std::string& Oid, const std::string& Value, const uint32_t Index)
	{
		setOid& o = oids.emplace_back();
		o.index = Index;
		o.oid = std::stoul(Oid);
		o.uuid = uuid;
		o.value = Value;
	}

	void Clear()
	{
		oids.clear();
	}
};

static void to_json(nlohmann::json& j, const setOids& mdl)
{

	j = nlohmann::json::object();
	j["setOids"] = nlohmann::json::array();

	for (const auto& elm : mdl.oids)
	{
		nlohmann::json jelm = nlohmann::json::object();
		jelm["oid"] = elm.oid;
		jelm["index"] = elm.index;
		jelm["uuid"] = elm.uuid;
		jelm["value"] = elm.value;
		j["setOids"].push_back(jelm);
	}


	//j["setOids"]["uuid"] = mdl.uuid;
}


struct ping
{
	std::string productName;
	std::string uuid;
};

static void to_json(nlohmann::json& j, const ping& mdl)
{
	j = nlohmann::json::object();
	j["ping"]["productName"] = mdl.productName;
	j["ping"]["uuid"] = mdl.uuid;
}

