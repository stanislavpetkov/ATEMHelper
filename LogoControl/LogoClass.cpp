#include "LogoClass.h"
#include "../Common/WebsocketClient/easywsclient.hpp"
#include <fmt/format.h>



enum class wscmd
{
	ping,
	setLogoPath1,
	setLogoPath2,
	setLogoPathBoth
};

struct cmdPack
{
	wscmd cmd;
	LogoSel logoSel; //-1 is logo off
	LogoSel logoSel2; //-1 is logo off
};

static std::string trim_end(const std::string& s)
{
	auto res = s;
	while (!res.empty())
	{
		if ((res.back() == '\r')
			|| (res.back() == '\n')
			|| (res.back() == ' '))
		{


			res.erase(res.begin() + res.size() - 1);
			continue;
		}
		break;
	}
	return res;
}

static std::vector<bool> convertLogoPathData(const std::vector<int>& value) noexcept
{
	try
	{
		std::vector<bool> ret; ret.reserve(value.size());
		for (const auto& elm : value)
		{
			try
			{

				ret.push_back(elm != 0);
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
static std::string GetDeviceInfo()
{
	return "{ \"devinfo\":\"\" }";
}

static std::string GetLogoPathStat(const std::string& logoPathId, const std::string& CardID)
{
	return "{ \"getOid\": { \"oid\": \"" + logoPathId + "\" ,\"uuid\": \"" + CardID + "\"} }";
}


static std::string makePing(const std::string& cardID)
{

	return "{ \"ping\":\"\" }";
}
static std::shared_ptr<easywsclient::WebSocket> buildWebSocket(const std::string& ip_)
{
	return  std::shared_ptr< easywsclient::WebSocket>(easywsclient::WebSocket::from_url(fmt::format("ws://{}:9002?protocolversion=3.0", ip_)));
}

static std::string getString(const nlohmann::json& j)
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


bool LogoClass::handleDeviceInfo(nlohmann::json& data)
{
	bool LogoPath1Found = false;
	bool LogoPath2Found = false;
	bool UserProductName = false;
	bool RealProductName = false;

	struct tmpLogo
	{
		std::string id;
		std::vector<bool> data;
	};
	std::vector<tmpLogo> logo_ids;


	for (auto it = data.begin(); it != data.end(); it++)
	{
		//Log::info(__func__, "{}", it.key());

		const nlohmann::json& jv = it.value();
		auto name = jv.find("name");
		auto oid = it.key();
		if (name == jv.end()) continue;

		std::string nameS = name->get<std::string>();
		const auto data_value_it = jv.find("data_value");
		if (data_value_it == jv.end()) continue;
		const auto data_value = data_value_it.value();

		if (nameS == "Card Time")
		{
			cardTimeId = oid;
			cardTime = data_value.get<std::string>();
			continue;
		}

		if (nameS == "CPU Usage")
		{
			cpuUsageId = oid;
			cpuUsage = data_value.get<std::string>();
			continue;
		}


		if (nameS == "Card Temp Front")
		{
			cardTempFrontId = oid;
			cardTempFront = data_value.get<std::string>();
			continue;
		}

		if (nameS == "Card Temp Rear")
		{
			cardTempRearId = oid;
			cardTempRear = data_value.get<std::string>();
			continue;
		}

		if (nameS == "RAM Usage")
		{
			ramUsageId = oid;
			ramUsage = data_value.get<std::string>();
			continue;
		}


		if (nameS == "Reference")
		{
			referenceId = oid;
			referenceStat = data_value.get<std::string>();
			continue;
		}


		if (nameS == "Input A")
		{
			inputAId = oid;
			inputAStat = data_value.get<std::string>();
			continue;
		}

		if (nameS == "Logo Enable")
		{



			tmpLogo t{};
			t.id = oid;
			t.data = convertLogoPathData(data_value);
			logo_ids.push_back(t);

			continue;
		}

		if (nameS == "Name override")
		{

			UserProductName = true;
			UpdateData(data_value.get<std::string>(), title_);
			continue;
		}

		if (nameS == "Product")
		{
			RealProductName = true;
			UpdateData(data_value.get<std::string>(), productModel_);

			continue;
		}

	}


	std::sort(logo_ids.begin(), logo_ids.end(), [](const tmpLogo& a, const tmpLogo& b) {
		int32_t aO = std::stol(a.id);
		int32_t bO = std::stol(b.id);

		return aO < bO;
		});

	if (logo_ids.size() >= 1)
	{
		LogoPath lp{};
		lp.id = logo_ids[0].id;
		lp.data = logo_ids[0].data;
		UpdateData(lp, path1);
		LogoPath1Found = true;
	}
	if (logo_ids.size() >= 2)
	{
		LogoPath lp{};
		lp.id = logo_ids[1].id;
		lp.data = logo_ids[1].data;
		UpdateData(lp, path2);
		LogoPath2Found = true;
	}


	return LogoPath2Found && LogoPath1Found && RealProductName && UserProductName;
}

void LogoClass::handleLogoPathChanged(const size_t logoPathNo, const std::vector<int>& value)
{


	if (logoPathNo == 1)
	{
		UpdateData(convertLogoPathData(value), path1.data);
	}
	else if (logoPathNo == 2)
	{
		UpdateData(convertLogoPathData(value), path2.data);
	}

	Log::info(__func__, "LogoPath {} changed", logoPathNo);
}

void LogoClass::commsFn()
{
	using namespace easywsclient;
	std::shared_ptr<WebSocket> ws = nullptr;
	int64_t lastPing = 0;

	bool bWaitingForDeviceInfo = true;
	std::string local_ip = {};
	{
		atomic_lock lock(data_lock_);
		local_ip = ip_;
	}

	while (!bStop)
	{

		{
			atomic_lock lock(data_lock_);
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
			ws = buildWebSocket(ip_);
			if (ws == nullptr)
			{
				std::this_thread::sleep_for(std::chrono::seconds(1));
				continue;
			}
			ws->send(GetDeviceInfo());
		}

		while (ws->getReadyState() != WebSocket::states::CLOSED) {
			if (bStop) {
				ws->close();
				break;
			}

			if (!bWaitingForDeviceInfo)
			{
				std::shared_ptr<cmdPack> cmd;

				std::string CardUUID;
				{
					atomic_lock lock(data_lock_);
					CardUUID = cardUUID;
				}
				while (tx_queue.try_pop(cmd))
				{

					switch (cmd->cmd)
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

						setOid so;
						so.uuid = CardUUID;
						so.index = 0;
						so.oid = std::stoul(path1.id);
						so.value = nlohmann::json::array();
						so.value.push_back((cmd->logoSel == LogoSel::logo1On) ? 1 : 0);
						so.value.push_back((cmd->logoSel == LogoSel::logo2On) ? 1 : 0);
						so.value.push_back((cmd->logoSel == LogoSel::logo3On) ? 1 : 0);
						so.value.push_back((cmd->logoSel == LogoSel::logo4On) ? 1 : 0);
						nlohmann::json j = so;

						ws->send(j.dump());




						break;
					}
					case wscmd::setLogoPath2:
					{
						setOid so;
						so.uuid = CardUUID;
						so.index = 0;
						so.oid = std::stoul(path2.id);
						so.value = nlohmann::json::array();
						so.value.push_back((cmd->logoSel == LogoSel::logo1On) ? 1 : 0);
						so.value.push_back((cmd->logoSel == LogoSel::logo2On) ? 1 : 0);
						so.value.push_back((cmd->logoSel == LogoSel::logo3On) ? 1 : 0);
						so.value.push_back((cmd->logoSel == LogoSel::logo4On) ? 1 : 0);

						nlohmann::json j = so;
						ws->send(j.dump());
						break;
					}

					case wscmd::setLogoPathBoth:
					{
						setOids soids{};
						soids.uuid = CardUUID;
						{
							setOid so;
							so.uuid = CardUUID;
							so.index = 0;
							so.oid = std::stoul(path1.id);
							so.value = nlohmann::json::array();
							so.value.push_back((cmd->logoSel == LogoSel::logo1On) ? 1 : 0);
							so.value.push_back((cmd->logoSel == LogoSel::logo2On) ? 1 : 0);
							so.value.push_back((cmd->logoSel == LogoSel::logo3On) ? 1 : 0);
							so.value.push_back((cmd->logoSel == LogoSel::logo4On) ? 1 : 0);
							soids.oids.push_back(so);
						}



						{
							setOid so;
							so.uuid = CardUUID;
							so.index = 0;
							so.oid = std::stoul(path2.id);
							so.value = nlohmann::json::array();
							so.value.push_back((cmd->logoSel2 == LogoSel::logo1On) ? 1 : 0);
							so.value.push_back((cmd->logoSel2 == LogoSel::logo2On) ? 1 : 0);
							so.value.push_back((cmd->logoSel2 == LogoSel::logo3On) ? 1 : 0);
							so.value.push_back((cmd->logoSel2 == LogoSel::logo4On) ? 1 : 0);
							soids.oids.push_back(so);
						}

						nlohmann::json j = soids;
						ws->send(j.dump());
						break;
					}
					default:
						Log::warn(__func__, "Unrecognized command!!! {}", static_cast<uint32_t>(cmd->cmd));
						break;
					}



				}
			}
			ws->poll(5);


			ws->dispatch([this, &ws, &lastPing, &bWaitingForDeviceInfo](const std::string& message) {

				if (message.empty()) return;

				if ((ClockHighRes::get_time() - lastPing) > ClockHighRes::get_time_base())
				{
					ws->sendPing();
					lastPing = ClockHighRes::get_time();
				}

				try
				{
					//if (message.find("4407") != std::string::npos)
					//{
					//	Log::info(__func__, "Message rcvd: {}", message);
					//}
					nlohmann::json j = nlohmann::json::parse(message);
					if (!j.is_object())
					{
						Log::error(__func__, "Message is not json object: {}", message);
						return;
					}

					auto isSetResult = j.find("setResult");
					if (isSetResult != j.end())
					{
						setResult sr = j;
						Log::info(__func__, "SetOK Received: {}", message);
						if (std::to_string(sr.oid) == path1.id)
						{

							if (sr.setRes == "setOK")
							{

								std::vector<bool> lres;
								for (const auto& e : sr.value)
								{
									lres.push_back(e.get<uint32_t>() != 0);
								}
								UpdateData(lres, path1.data);
							}



						}
						else
							if (std::to_string(sr.oid) == path2.id)
							{
								if (sr.setRes == "setOK")
								{
									std::vector<bool> lres;
									for (const auto& e : sr.value)
									{
										lres.push_back(e.get<uint32_t>() != 0);
									}
									UpdateData(lres, path2.data);
								}
							}



					}

					const auto devInfoIt = j.find("devinfo_elems");
					if (devInfoIt != j.end())
					{

						UpdateData(j.find("uuid")->get<std::string>(), cardUUID);

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
							//#ifdef _DEBUG
							//							if (elm.second.is_string())
							//							{
							//								Log::info(__func__, "Update For: {} - {}", elm.first, elm.second.get<std::string>());
							//							}
							//							else
							//							{
							//								Log::info(__func__, "Update For: {}", elm.first);
							//							}
							//#endif

							if (elm.first == path1.id)
							{
								handleLogoPathChanged(1, elm.second);
							}
							else
								if (elm.first == path2.id)
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
							//#ifdef _DEBUG
							//							if (elm.data_value.is_string())
							//							{
							//								Log::info(__func__, "Update For: {} - {}", elm.oid, elm.data_value.get<std::string>());
							//							}
							//							else
							//							{
							//								Log::info(__func__, "Update For: {}", elm.oid);
							//							}
							//#endif

							if (elm.oid == path1.id)
							{
								handleLogoPathChanged(1, elm.data_value);
							}
							else
								if (elm.oid == path2.id)
								{
									handleLogoPathChanged(2, elm.data_value);
								}
								else if (elm.oid == cardTimeId)
								{
									UpdateDataNOLT(trim_end(elm.data_value.get<std::string>()), cardTime);
								}
								else if (elm.oid == referenceId)
								{
									UpdateData(trim_end(elm.data_value.get<std::string>()), referenceStat);
								}
								else if (elm.oid == cpuUsageId)
								{
									UpdateDataNOLT(trim_end(elm.data_value.get<std::string>()), cpuUsage);
								}
								else if (elm.oid == cardTempFrontId)
								{
									UpdateData(trim_end(elm.data_value.get<std::string>()), cardTempFront);
								}
								else if (elm.oid == cardTempRearId)
								{
									UpdateData(trim_end(elm.data_value.get<std::string>()), cardTempRear);
								}
								else if (elm.oid == ramUsageId)
								{
									UpdateDataNOLT(trim_end(elm.data_value.get<std::string>()), ramUsage);
								}
								else if (elm.oid == inputAId)
								{
									UpdateData(trim_end(elm.data_value.get<std::string>()), inputAStat);
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

					auto setOids_ = j.find("setOids");
					if (setOids_ != j.end())
					{
						for (const auto& jelm : *setOids_)
						{
							const auto error = jelm.find("error");

							if (error != jelm.end())
							{
								Log::error(__func__, "Error message received: {}", message);
								return;
							}

							const auto oid = getString(*jelm.find("oid"));
							const auto value = *jelm.find("value");
							const auto index = getString(*jelm.find("index"));
							const auto result = jelm.find("setResult")->get<std::string>();


							if (result != "setOK")
							{
								Log::error(__func__, "oid: {}, index: {}, value: {}, setResult: {}", oid, index, value.dump(), result);
								continue;
							}

							if (oid == path1.id)
							{
								handleLogoPathChanged(1, value);
								continue;
							}

							if (oid == path2.id)
							{
								handleLogoPathChanged(2, value);
								continue;
							}
						}
						return;
					}

					Log::info(__func__, "Unhandled {}", message);


				}
				catch (const std::exception& ex)
				{
					Log::error(__func__, "Can not parse message::: {}. {}", message, ex.what());
					return;
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
LogoClass::LogoClass(const std::string& ip) :
	cardUUID("")
{
	comms = std::thread([this]() {return commsFn(); });
	Ip(ip);
}

LogoClass::~LogoClass()
{
	bStop = true;
	if (comms.joinable()) comms.join();
}

std::string LogoClass::GetInputAStatus()
{
	atomic_lock  l(data_lock_);
	return inputAStat;
}

std::string LogoClass::GetCardTemperatureFront()
{
	atomic_lock  l(data_lock_);
	return cardTempFront;
}

std::string LogoClass::GetCardTemperatureRear()
{
	atomic_lock  l(data_lock_);
	return cardTempRear;
}

std::string LogoClass::GetCPUUsage()
{
	atomic_lock  l(data_lock_);
	return cpuUsage;
}

std::string LogoClass::GetRAMUsage()
{
	atomic_lock  l(data_lock_);
	return ramUsage;
}

std::string LogoClass::GetCardTime()
{
	atomic_lock  l(data_lock_);
	return cardTime;
}

std::string LogoClass::GetReferenceStatus()
{
	atomic_lock  l(data_lock_);
	return referenceStat;
}

int64_t LogoClass::GetLastUpdateTime()
{
	atomic_lock lock(data_lock_);

	return last_update_time;
}

bool LogoClass::SetLogo(const uint32_t pathNo, const LogoSel sel)
{
	if ((pathNo != 1) && (pathNo != 2)) return false;

	auto cmd = std::make_shared<cmdPack>();
	cmd->cmd = pathNo == 1 ? wscmd::setLogoPath1 : wscmd::setLogoPath2;
	cmd->logoSel = sel;
	tx_queue.push(cmd);
	return true;
}

std::string LogoClass::Id()
{
	atomic_lock  l(data_lock_);
	return cardUUID;
}

std::string LogoClass::Model()
{
	atomic_lock  l(data_lock_);
	return productModel_;
}

std::string LogoClass::Title()
{
	atomic_lock  l(data_lock_);
	return title_;
}

std::string LogoClass::Ip()
{
	atomic_lock  l(data_lock_);
	return ip_;
}

void LogoClass::Ip(const std::string& ipaddr)
{
	atomic_lock  l(data_lock_);
	ip_ = ipaddr;
}

std::vector<bool> LogoClass::GetLogosStatus(const uint32_t pathNo)
{
	if ((pathNo < 1) || (pathNo > 2)) return {};

	atomic_lock  l(data_lock_);
	return pathNo == 1 ? path1.data : path2.data;
}

bool LogoClass::SetLogos(const LogoSel selPath1, const LogoSel selPath2)
{

	auto cmd = std::make_shared<cmdPack>();
	cmd->cmd = wscmd::setLogoPathBoth;
	cmd->logoSel = selPath1;
	cmd->logoSel2 = selPath2;
	tx_queue.push(cmd);
	return true;
}