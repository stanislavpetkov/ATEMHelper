#pragma once
#include <memory>
#include <WinSock2.h>
#include "../Common/HTTP Server/HTTPServer2.h"
#include "LogoClass.h"
#include "BoardAPIModel.h"

class ControllerClass
{
	
private:
	std::atomic<bool> veclock;
	std::vector<std::shared_ptr<LogoClass>> logoBoards;
	std::unique_ptr< HTTP::HTTPServer2> server;
	std::vector<BoardAPIModel> GetBoards();
	HTTP::StatusCode GetBoards(const HTTP::HTTP_IOREQUEST& request, HTTP::HTTP_REPLY& reply);
	HTTP::StatusCode SetLogoAPI(const HTTP::HTTP_IOREQUEST& request, HTTP::HTTP_REPLY& reply);
	HTTP::StatusCode GetNotificationsLongPoling(const HTTP::HTTP_IOREQUEST& request, HTTP::HTTP_REPLY& reply);
public:
	ControllerClass();
	~ControllerClass();
};

