// LogoControl.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include "LogoClass.h"
#include "ControllerClass.h"
#include <thread>
#include <WinSock2.h>


int main()
{

	INT rc;
	WSADATA wsaData;

	rc = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (rc) {
		throw std::exception("");
	}


	ControllerClass ctrlr;

	

	size_t test = 1;
	while (true)
	{

		

	

		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}
}
