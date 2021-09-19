#include <atomic>
#include <thread>
#include "LogoClass.h"
#include "../Common/atomic_lock.h"
#include "../Common/WebsocketClient/easywsclient.hpp"
#include "../Common/ThirdParty/nlohmann_json/src/json.hpp"
#include "../Common/Clock/Clock.h"
#include <fmt/format.h>
#include <concurrent_queue.h>
#include "../LibLogging/LibLogging.h"


std::string getString(const nlohmann::json& j)
{
	if (j.is_string())
	{
		return j.get<std::string>();
	}
	if (j.is_number_integer())
	{
		auto X = j.get<int64_t>();
		return std::to_string(X);
	}
	if (j.is_number())
	{
		auto X = j.get<double>();
		return std::to_string(X);
	}

	throw std::exception("Incompatible type");
}
enum class wscmd
{
	ping,
	setLogoPath1,
	setLogoPath2
};

struct cmdPack
{
	wscmd cmd;
	int32_t logoNo; //-1 is logo off
};

struct pupdates
{
	std::map<std::string, nlohmann::json> oids;
	std::string uuid;
};

void from_json(const nlohmann::json& j, pupdates& mdl)
{
	if (!j.is_object()) throw std::exception("Invalid value");

	auto uuid = j.find("uuid");
	auto oids = j.find("oids");
	mdl.uuid = uuid->get<std::string>();


	mdl.oids = oids->get<std::map<std::string, nlohmann::json>>();

}

struct updates_packet
{
	pupdates updates;
};

void from_json(const nlohmann::json& j, updates_packet& mdl)
{
	if (!j.is_object()) throw std::exception("Invalid value");

	auto updates = j.find("updates");

	mdl.updates = updates->get<pupdates>();

}


struct data_updates_packet
{
	pupdates updates;
};

void from_json(const nlohmann::json& j, data_updates_packet& mdl)
{
	if (!j.is_object()) throw std::exception("Invalid value");

	auto updates = j.find("data_updates");

	mdl.updates = updates->get<pupdates>();

}



struct setOid
{
	std::string oid;
	nlohmann::json value;
	std::string index = "0";
	std::string uuid;
};

void to_json(nlohmann::json& j, const setOid& mdl)
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

	void AddValue(const std::string& Oid, const std::string& Value, const std::string& Index)
	{
		setOid& o = oids.emplace_back();
		o.index = Index;
		o.oid = Oid;
		o.uuid = uuid;
		o.value = Value;
	}
	void Clear()
	{
		oids.clear();
	}
};

void to_json(nlohmann::json& j, const setOids& mdl)
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

void to_json(nlohmann::json& j, const ping& mdl)
{
	j = nlohmann::json::object();
	j["ping"]["productName"] = mdl.productName;
	j["ping"]["uuid"] = mdl.uuid;
}

struct LogoClass::impl
{
	
	concurrency::concurrent_queue<cmdPack> tx_queue;
	std::atomic<bool> bStop;



	std::atomic<bool> data_lock = false;
	std::string title_;
	std::string cardUUID = "";
	std::string ip_;
	std::string productModel_;
	std::thread comms;
	std::vector<bool> logoPath1Data;
	std::vector<bool> logoPath2Data;
	std::string logoPath1InserterId;
	std::string logoPath2InserterId;



	bool EnableLogo(const uint32_t pathNo, const uint32_t logoNo)
	{
		if ((pathNo != 1) && (pathNo != 2)) return false;

		cmdPack cmd{};
		cmd.cmd = pathNo == 1 ? wscmd::setLogoPath1 : wscmd::setLogoPath2;
		cmd.logoNo = logoNo;
		tx_queue.push(cmd);
		return true;
	}



	std::string Id()
	{
		atomic_lock  l(data_lock);
		return cardUUID;
	}
	std::string Model()
	{
		atomic_lock  l(data_lock);
		return productModel_;
	}

	std::string Title()
	{
		atomic_lock  l(data_lock);
		return title_;
	}

	std::string Ip()
	{
		atomic_lock  l(data_lock);
		return ip_;
	}


	void Ip(const std::string& ipaddr)
	{
		atomic_lock  l(data_lock);
		ip_ = ipaddr;
	}
	std::vector<bool> GetLogosStatus(const uint32_t pathNo)
	{
		atomic_lock  l(data_lock);
		if (pathNo == 1)
		{
			return logoPath1Data;
		}
		if (pathNo == 2)
		{
			return logoPath2Data;
		}
		return {};
	}
	std::shared_ptr<easywsclient::WebSocket> buildWebSocket()
	{
		return  std::shared_ptr< easywsclient::WebSocket>(easywsclient::WebSocket::from_url(fmt::format("ws://{}:9002?protocolversion=3.0", ip_)));
	}

	std::string GetDeviceInfo()
	{
		return "{ \"devinfo\":\"\" }";
	}

	std::string GetLogoPathStat(const std::string& logoPathId, const std::string& CardID)
	{
		return "{ \"getOid\": { \"oid\": \"" + logoPathId + "\" ,\"uuid\": \"" + CardID + "\"} }";
	}


	std::string makePing(const std::string& cardID)
	{
		
		return "{ \"ping\":\"\" }";
	}


	size_t NumLogos()
	{
		atomic_lock lock(data_lock);
		return std::max( logoPath1Data.size(), logoPath2Data.size());
	}

	bool handleDeviceInfo(nlohmann::json& data)
	{
		bool LogoPath1Found = false;
		bool LogoPath2Found = false;
		bool UserProductName = false;
		bool RealProductName = false;
		for (auto it = data.begin(); it != data.end(); it++)
		{
			//Log::info(__func__, "{}", it.key());

			const nlohmann::json& jv = it.value();
			auto name = jv.find("oid_name");
			auto oid = it.key();
			if (name == jv.end()) continue;

			std::string nameS = name->get<std::string>();
			if (nameS == "OID_GRAPHIC_OVERLAY_SLATE_ENABLE1")
			{

				const auto data_value = jv.find("data_value");
				if (data_value != jv.end())
				{
					atomic_lock lock(data_lock);
					LogoPath1Found = true;
					logoPath1Data = convertLogoPathData(*data_value);
					logoPath1InserterId = oid;
				}
				continue;
			}
			if (nameS == "OID_GRAPHIC_OVERLAY_SLATE_ENABLE2")
			{
				const auto data_value = jv.find("data_value");
				if (data_value != jv.end())
				{
					atomic_lock lock(data_lock);
					LogoPath2Found = true;
					logoPath2Data = convertLogoPathData(*data_value);
					logoPath2InserterId = oid;
				}
				continue;
			}
			if (nameS == "OID_USER_PRODUCT_NAME")
			{
				const auto data_value = jv.find("data_value");
				if (data_value != jv.end())
				{
					atomic_lock lock(data_lock);
					UserProductName = true;
					title_ = data_value->get<std::string>();
				}
				continue;
			}

			if (nameS == "OID_PRODUCT_NAME")
			{
				const auto data_value = jv.find("data_value");
				if (data_value != jv.end())
				{
					atomic_lock lock(data_lock);
					RealProductName = true;
					productModel_ = data_value->get<std::string>();
				}
				continue;
			}

		}

		return LogoPath2Found && LogoPath1Found && RealProductName && UserProductName;
	}


	std::vector<bool> convertLogoPathData(const std::vector<std::string>& value) noexcept
	{
		try
		{
			std::vector<bool> ret; ret.reserve(value.size());
			for (const auto& elm : value)
			{
				try
				{
					auto x = atol(elm.c_str());
					ret.push_back(x != 0);
				}
				catch (...)
				{
					ret.push_back(false);
				}

			}
			return ret;
		}
		catch (...)
		{
			return { false,false,false,false };
		}
	}

	void handleLogoPathChanged(const size_t logoPathNo, const std::vector<std::string>& value)
	{
		atomic_lock lock(data_lock);

		if (logoPathNo == 1)
		{
			logoPath1Data = convertLogoPathData(value);
		}
		else if (logoPathNo == 2)
		{
			logoPath2Data = convertLogoPathData(value);
		}


		Log::info(__func__, "LogoPath {} changed", logoPathNo);



	}


	void handleLogoPathChanged(const size_t logoPathNo, const std::string & index, const std::string & value)
	{
		size_t iIndex = atol(index.c_str());
		size_t iValue = atol(value.c_str());

		atomic_lock lock(data_lock);

		if (logoPathNo == 1)
		{
			if (iIndex < logoPath1Data.size())
			{
				logoPath1Data[iIndex] = iValue != 0;
			}
			
		}
		else if (logoPathNo == 2)
		{
			if (iIndex < logoPath2Data.size())
			{
				logoPath2Data[iIndex] = iValue != 0;
			}
		}


		Log::info(__func__, "LogoPath {} changed", logoPathNo);



	}

	void commsFn()
	{
		using namespace easywsclient;
		std::shared_ptr<WebSocket> ws = nullptr;
		int64_t lastPing = 0;
		
		bool bWaitingForDeviceInfo = true;
		std::string local_ip = {};
		{
			atomic_lock lock(data_lock);
			local_ip = ip_;
		}

		while (!bStop)
		{

			{
				atomic_lock lock(data_lock);
				if (local_ip != ip_)
				{
					ws = nullptr;
					local_ip = ip_;
				}
			}

			if (ws == nullptr)
			{
				if (ip_.empty())
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(50));
					continue;
				}
				bWaitingForDeviceInfo = true;
				tx_queue.clear();
				ws = buildWebSocket();
				if (ws == nullptr)
				{
					std::this_thread::sleep_for(std::chrono::seconds(1));
					continue;
				}
				ws->send(GetDeviceInfo());
			}

			while (ws->getReadyState() != WebSocket::CLOSED) {
				if (bStop) {
					ws->close();
					break;
				}

				if (!bWaitingForDeviceInfo)
				{
					cmdPack cmd;

					std::string CardUUID;
					{
						atomic_lock lock(data_lock);
						CardUUID = cardUUID;
					}
					while (tx_queue.try_pop(cmd))
					{
						
						switch (cmd.cmd)
						{
						case wscmd::ping:
						{
							ping data;
							data.productName = productModel_;
							data.uuid = CardUUID;
							nlohmann::json j = data;
							ws->send(j.dump());
							break;
						}
						case wscmd::setLogoPath1:
						{
							setOids so;
							so.uuid = CardUUID;
							size_t ndx = 0;
							for (const auto& e : logoPath1Data)
							{

								so.AddValue(logoPath1InserterId, ndx == (cmd.logoNo - 1) ? "1" : "0", std::to_string(ndx));
								ndx++;
							}
							nlohmann::json j = so;
							ws->send(j.dump());
							break;
						}
						case wscmd::setLogoPath2:
						{
							setOids so;
							so.uuid = CardUUID;
							size_t ndx = 0;
							for (const auto& e : logoPath2Data)
							{

								so.AddValue(logoPath2InserterId, ndx == (cmd.logoNo - 1) ? "1" : "0", std::to_string(ndx));
								ndx++;
							}
							nlohmann::json j = so;
							ws->send(j.dump());
							break;
						}
						default:
							Log::warn(__func__, "Unrecognized command!!! {}", static_cast<uint32_t>(cmd.cmd));
							break;
						}



					}
				}
				ws->poll(5);


				ws->dispatch([this, &ws, &lastPing,  &bWaitingForDeviceInfo](const std::string& message) {

					if (message.empty()) return;

					if ((ClockHighRes::get_time() - lastPing) > ClockHighRes::get_time_base())
					{
						ws->sendPing();
						lastPing = ClockHighRes::get_time();
					}

					try
					{
						nlohmann::json j = nlohmann::json::parse(message);
						if (!j.is_object())
						{
							Log::error(__func__, "Message is not json object: {}", message);
							return;
						}

						const auto devInfoIt = j.find("devinfo_elems");
						if (devInfoIt != j.end())
						{
							{
								atomic_lock lock(data_lock);
								cardUUID = j.find("uuid")->get<std::string>();
							}
							if (handleDeviceInfo(*devInfoIt))
							{
								bWaitingForDeviceInfo = false;

							}
							else
							{
								ws->send(GetDeviceInfo());
							}
							return;
						}




						auto updates = j.find("data_updates");
						if (updates != j.end())
						{
							data_updates_packet up = j;


							for (const auto& elm : up.updates.oids)
							{

								if (elm.second.is_string())
								{
									Log::info(__func__, "Update For: {} - {}", elm.first, elm.second.get<std::string>());
								}
								else
								{
									Log::info(__func__, "Update For: {}", elm.first);
								}


								if (elm.first == logoPath1InserterId)
								{
									handleLogoPathChanged(1, elm.second);
								}
								else
									if (elm.first == logoPath2InserterId)
									{
										handleLogoPathChanged(2, elm.second);
									}
							}
							return;
						};



						updates = j.find("updates");
						if (updates != j.end())
						{
							updates_packet up = j;


							for (const auto& elm : up.updates.oids)
							{

								if (elm.second.is_string())
								{
									Log::info(__func__, "Update For: {} - {}", elm.first, elm.second.get<std::string>());
								}
								else
								{
									Log::info(__func__, "Update For: {}", elm.first);
								}


								if (elm.first == logoPath1InserterId)
								{
									handleLogoPathChanged(1, elm.second);
								}
								else
									if (elm.first == logoPath2InserterId)
									{
										handleLogoPathChanged(2, elm.second);
									}
							}
							return;
						}

						const auto setResult = j.find("setResult");
						if (setResult != j.end())
						{

							const auto oid = j.find("oid");

							const auto value = j.find("value");

							if ((oid != j.end()) && (setResult != j.end()) && (value != j.end()))
							{
								const int64_t oidI = oid->get<int64_t>();
								const std::string setResultS = setResult->get<std::string>();




							}
							return;
						}

						auto setOids_= j.find("setOids");
						if (setOids_ != j.end())
						{
							for (const auto& jelm : *setOids_)
							{
								const auto oid = getString(*jelm.find("oid"));
								const auto value = getString(*jelm.find("value"));
								const auto index = getString(*jelm.find("index"));
								const auto result = jelm.find("setResult")->get<std::string>();

								
								if (result != "setOK")
								{
									Log::error(__func__, "oid: {}, index: {}, value: {}, setResult: {}", oid, index, value, result);
									continue;
								}

								if (oid == logoPath1InserterId)
								{
									handleLogoPathChanged(1,index,value);
									continue;
								}
								
								if (oid == logoPath2InserterId)
								{
									handleLogoPathChanged(2, index, value);
									continue;
								}
							}
							return;
						}

						Log::info(__func__, "Unhandled {}", message);


					}
					catch (...)
					{
						Log::error(__func__, "Can not parse message::: {}", message);
						return;
					}



			});
		}

		ws = nullptr;



	}
}


impl(const std::string& ip);
~impl();
};

LogoClass::impl::impl(const std::string& ip) :
	cardUUID(ip)
{


	comms = std::thread([this]() {return commsFn(); });

	Ip(ip);
}

LogoClass::impl::~impl()
{
	bStop = true;
	if (comms.joinable()) comms.join();
}




LogoClass::LogoClass(const std::string& ip) :
	impl_(std::make_shared<impl>(ip))
{
}

LogoClass::~LogoClass()
{
	impl_ = nullptr;
}

std::string LogoClass::Id() const
{
	return impl_->Id();
}

std::string LogoClass::Title() const
{
	return impl_->Title();
}

std::string LogoClass::Ip() const
{
	return impl_->Ip();
}

std::string LogoClass::Model() const
{
	return impl_->Model();
}

size_t LogoClass::NumLogos() const
{
	return impl_->NumLogos();
}

std::vector<bool> LogoClass::GetLogosStatus(const uint32_t pathNo)
{
	return impl_->GetLogosStatus(pathNo);
}

void LogoClass::Ip(const std::string& ipaddr)
{
	return impl_->Ip(ipaddr);
}

bool LogoClass::EnableLogo(const uint32_t pathNo, const uint32_t logoNo)
{


	return impl_->EnableLogo(pathNo, logoNo);
}
