#include <atomic>
#include <vector>
#include <WinSock2.h>
#include "../Common/atomic_lock.h"
#include "../Common/HTTP Server/HTTPServer2.h"
#include "ControllerClass.h"
#include "LogoClass.h"
#include "BoardAPIModel.h"
#include "../Common/sys_utils.h"

struct ControllerClass::impl
{
	std::atomic<bool> veclock;
	std::vector<std::shared_ptr<LogoClass>> logoBoards;
	std::unique_ptr< HTTP::HTTPServer2> server;

	HTTP::StatusCode GetBoards(const HTTP::HTTP_IOREQUEST& request, HTTP::HTTP_REPLY& reply)
	{
		std::vector<BoardAPIModel> mdl;
		reply.type = HTTP::Reply_type_t::rt_JSON;

		{
			atomic_lock lock(veclock);
			for (auto& elm : logoBoards)
			{
				BoardAPIModel & api = mdl.emplace_back();

				auto logo1path = elm->GetLogosStatus(1);
				auto logo2path = elm->GetLogosStatus(2);
				const auto numLogos = elm->NumLogos() / 2;
				api.activeLogo = -1;
				if (logo1path.size() == logo2path.size())
				{
					for (size_t i = 0; i < numLogos; i++)
					{
						if (logo1path[i] && logo2path[i + 2])
						{
							api.activeLogo = static_cast<int32_t>(i)+1;
							break;
						}
					}
				}


				api.id = elm->Id();
				api.model = elm->Model();
				api.name = elm->Title();
				api.numLogos = static_cast<uint32_t>(numLogos); //Only half of the logos available
			}
		}
		nlohmann::json j = mdl;
		reply.message = j.dump();
		return HTTP::StatusCode::success_ok;
	}

	/*static HTTP::StatusCode GetBoard(const uint64_t cookie, const HTTP::HTTP_IOREQUEST& request, HTTP::HTTP_REPLY& reply)
	{
		vv
	}*/



	impl() :
		server(std::make_unique<HTTP::HTTPServer2>(reinterpret_cast<uint64_t>(this), "0.0.0.0", 9012, "LogoAPI", false))
	{

	logoBoards.push_back(std::make_shared<LogoClass>("192.168.16.139"));
	logoBoards.push_back(std::make_shared<LogoClass>("192.168.16.140"));

		INT rc;
		WSADATA wsaData;

		rc = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (rc) {
			throw std::exception("");
		}


		pbn::fs::Path contentPath(pbn::OS::get_current_module_path());
		server->set_document_root(contentPath.parent_path().append("content"));


		server->add_endpoint(HTTP::methods::s_get_method, "/api/boards", [this](const uint64_t cookie, const HTTP::HTTP_IOREQUEST& request, HTTP::HTTP_REPLY& reply) {return  GetBoards(request, reply); });


		server->Start();
	}

	~impl()
	{

		server = nullptr;

		{
			atomic_lock lock(veclock);
			logoBoards.clear();

		}


		WSACleanup();
	}
};

ControllerClass::ControllerClass() :
	impl_(std::make_shared<impl>())
{
}

ControllerClass::~ControllerClass()
{
	impl_ = nullptr;
}
