#include <atomic>
#include <vector>
#include <fstream>

#include "../Common/atomic_lock.h"

#include "ControllerClass.h"

#include "../Common/sys_utils.h"
#include "../LibLogging/LibLogging.h"

std::vector<BoardAPIModel> ControllerClass::GetBoards()
{
	std::vector<BoardAPIModel> result{};
	{
		atomic_lock lock(veclock);
		for (auto& elm : logoBoards)
		{
			if (elm->Id().empty()) continue;
			BoardAPIModel& api = result.emplace_back();
			api.last_update_time = elm->GetLastUpdateTime();
			auto logo1path = elm->GetLogosStatus(1);
			auto logo2path = elm->GetLogosStatus(2);

			auto numLogos = std::min(logo1path.size(), logo2path.size()) / 2;


			api.activeLogo = -1;
			if ((logo1path.size() == logo2path.size()) && (logo1path.size() == 4))
			{
				auto logo1On = (logo1path[0] && logo2path[0 + 2]);
				auto logo2On = (logo1path[1] && logo2path[1 + 2]);

				auto invalid = logo1path[2] || logo1path[3] ||
					logo2path[0] || logo2path[1];

				invalid |= (logo1path[0] && logo1path[1]);
				invalid |= (logo2path[2] && logo2path[3]);

				if (logo1path[0] != logo2path[0 + 2]) invalid = true;
				if (logo1path[1] != logo2path[1 + 2]) invalid = true;

				if (!invalid)
				{
					if (logo1On && !logo2On) {
						api.activeLogo = 1;
						api.activeLogoTxt = "logo 1";
					}
					if (!logo1On && logo2On) {
						api.activeLogoTxt = "logo 2";
						api.activeLogo = 2;
					}
					if (!logo1On && !logo2On) {
						api.activeLogo = 0; //all logo off
						api.activeLogoTxt = "logo off";
					}


				}
				else
				{
					api.activeLogoTxt = "invalid logo set";
				}

			}

			api.cpuUsage = elm->GetCPUUsage();
			api.cardTemperatureFront = elm->GetCardTemperatureFront();
			api.cardTemperatureRear = elm->GetCardTemperatureRear();
			api.ramUsage = elm->GetRAMUsage();
			api.cardTime = elm->GetCardTime();
			api.referenceStat = elm->GetReferenceStatus();
			api.inputAStat = elm->GetInputAStatus();

			api.id = elm->Id();
			api.model = elm->Model();
			api.name = elm->Title();
			api.numLogos = static_cast<uint32_t>(numLogos); //Only half of the logos available
		}
	}
	return result;
}
HTTP::StatusCode ControllerClass::GetBoards(const HTTP::HTTP_IOREQUEST& request, HTTP::HTTP_REPLY& reply)
{
	std::vector<BoardAPIModel> mdl = GetBoards();
	reply.type = HTTP::Reply_type_t::rt_JSON;
	nlohmann::json j = mdl;
	reply.message = j.dump();
	return HTTP::StatusCode::success_ok;
}

HTTP::StatusCode ControllerClass::SetLogoAPI(const HTTP::HTTP_IOREQUEST& request, HTTP::HTTP_REPLY& reply)
{
	auto brdId = request.getUrlParam("{Id}");

	auto value = request.getUrlParam("logo");
	reply.type = HTTP::Reply_type_t::rt_JSON;
	if (!value.has_value())
	{
		return reply.DoError(HTTP::StatusCode::client_error_bad_request, ErrorResponse<HTTP::StatusCode>{ HTTP::StatusCode::client_error_bad_request, "parameter 'logo' not found" });
	}


	auto logo = value.value();
	if (logo.empty())
	{
		return reply.DoError(HTTP::StatusCode::client_error_bad_request, ErrorResponse<HTTP::StatusCode>{ HTTP::StatusCode::client_error_bad_request, "parameter 'logo' can not be empty" });
	}


	if (logo == "off") logo = "0";

	LogoSel logoS;

	try
	{
		const auto x = std::stoul(logo);
		if (x > uint32_t(LogoSel::logo2On))
		{
			return reply.DoError(HTTP::StatusCode::client_error_bad_request, ErrorResponse<HTTP::StatusCode>{ HTTP::StatusCode::client_error_bad_request, "parameter 'logo' invalid enum. 0,1,2 are valid" });
		}
		logoS = LogoSel(x);
	}
	catch (const std::exception&)
	{
		return reply.DoError(HTTP::StatusCode::client_error_bad_request, ErrorResponse<HTTP::StatusCode>{ HTTP::StatusCode::client_error_bad_request, "parameter 'logo' invalid enum. 0,1,2 are valid" });
	}



	{
		atomic_lock lock(veclock);
		for (auto& elm : logoBoards)
		{
			if (elm->Id() == brdId)
			{
				if (logoS == LogoSel::logoOff)
				{
					elm->SetLogos(LogoSel::logoOff, LogoSel::logoOff);
				}
				else
				{
					elm->SetLogos(logoS, LogoSel(uint32_t(logoS) + 2));
				}

				return reply.DoError(HTTP::StatusCode::success_ok, ErrorResponse<HTTP::StatusCode>{ HTTP::StatusCode::success_ok, "Success" });
			}
		}
	}

	return reply.DoError(HTTP::StatusCode::client_error_not_found, ErrorResponse<HTTP::StatusCode>{ HTTP::StatusCode::client_error_not_found, "BoardNotFound" });

}

HTTP::StatusCode ControllerClass::GetNotificationsLongPoling(const HTTP::HTTP_IOREQUEST& request, HTTP::HTTP_REPLY& reply)
{
	int64_t lastId = 0;
	try
	{
		auto last_id = request.getUrlParam("last_id");
		if (last_id.has_value())
		{
			lastId = (std::stoll(last_id.value()) * 10000ll)+ 10000ll;
		}
	}
	catch (const std::exception&)
	{
		return reply.DoError(HTTP::StatusCode::client_error_not_found, ErrorResponse<HTTP::StatusCode>{ HTTP::StatusCode::client_error_not_found, "last_id not a int value" });
	}
	
	auto start_time = ClockHighRes::get_time();

	if (abs(start_time - lastId) >= (10ll * ClockHighRes::get_time_base()))
	{
		lastId = 0;//force return
	}

	while (true)
	{
		auto now = ClockHighRes::get_time();
		auto boards = GetBoards();

		if (boards.empty())
		{
			if ((now - start_time) >= (10ll * ClockHighRes::get_time_base()) || (lastId==0) )
			{
				BoardsAPIModel mdl;
				mdl.timeStampMs = now / 10000ll; //millis
				mdl.boards = {};

				reply.type = HTTP::Reply_type_t::rt_JSON;
				nlohmann::json j = mdl;
				reply.message = j.dump();
				return HTTP::StatusCode::success_ok;
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

		int64_t max_upd = std::max_element(boards.begin(), boards.end(), [](const BoardAPIModel& a, const BoardAPIModel& b) {
			return a.last_update_time < b.last_update_time;
			})->last_update_time;


		if ((max_upd > lastId) ||
			((now - start_time) >= (10ll * ClockHighRes::get_time_base())))
		{
			BoardsAPIModel mdl;
			mdl.timeStampMs = max_upd / 10000ll; //millis
			mdl.boards = boards;

			reply.type = HTTP::Reply_type_t::rt_JSON;
			nlohmann::json j = mdl;
			reply.message = j.dump();
			return HTTP::StatusCode::success_ok;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
	//we need to wait




	return HTTP::StatusCode::success_no_content;
}



ControllerClass::ControllerClass() :
	server(std::make_unique<HTTP::HTTPServer2>(reinterpret_cast<uint64_t>(this), "0.0.0.0", 9012, "LogoAPI", false))
{
	pbn::fs::Path p = pbn::OS::GetProgramDataFolder();
	p.append("LogoControl");

	pbn::fs::create_dirs(p);
	p.append("config.json");
	if (!pbn::fs::file_exist(p))
	{
		std::ofstream ofs(p.wstring());
		ofs.unsetf(std::ios::skipws);
		nlohmann::json j = nlohmann::json::array();
		j.push_back("192.168.16.139");
		j.push_back("192.168.16.140");
		std::string jj = j.dump();
		ofs << jj;
		ofs.flush();
		ofs.close();
	}

	std::ifstream ifs(p.wstring());
	if (ifs.is_open())
	{
		try
		{
			nlohmann::json j = nlohmann::json::parse(ifs);
			if (!j.is_array()) throw std::runtime_error("Wrong json ... must be array of strings");
			for (const auto& elm : j)
			{
				if (!elm.is_string()) throw std::runtime_error("Wrong json ... must be array of strings");
				logoBoards.push_back(std::make_shared<LogoClass>(elm.get<std::string>()));
			}


		}
		catch (const std::exception& e)
		{
			Log::error(__func__, "Config file ::: {}", e.what());
		}
		catch (...)
		{
			Log::error(__func__, "Wrong config file format");

		}
	}







	pbn::fs::Path contentPath(pbn::OS::get_current_module_path());
#ifdef _DEBUG
	server->set_document_root(contentPath.parent_path().parent_path().append("content"));
#else

	server->set_document_root(contentPath.append("content"));
#endif


	server->add_endpoint(HTTP::methods::s_get_method, "/api/boards", [this](const uint64_t cookie, const HTTP::HTTP_IOREQUEST& request, HTTP::HTTP_REPLY& reply) {return  GetBoards(request, reply); });
	server->add_endpoint(HTTP::methods::s_post_method, "/api/boards/{Id}/logo", [this](const uint64_t cookie, const HTTP::HTTP_IOREQUEST& request, HTTP::HTTP_REPLY& reply) {return  SetLogoAPI(request, reply); });
	server->add_endpoint(HTTP::methods::s_get_method, "/api/boards/{Id}/logo", [this](const uint64_t cookie, const HTTP::HTTP_IOREQUEST& request, HTTP::HTTP_REPLY& reply) {return  SetLogoAPI(request, reply); });
	server->add_endpoint(HTTP::methods::s_get_method, "/api/boards/poll", [this](const uint64_t cookie, const HTTP::HTTP_IOREQUEST& request, HTTP::HTTP_REPLY& reply) {return  GetNotificationsLongPoling(request, reply); });



	server->Start();
}

ControllerClass::~ControllerClass()
{
	server = nullptr;

	{
		atomic_lock lock(veclock);
		logoBoards.clear();

	}


	WSACleanup();
}
