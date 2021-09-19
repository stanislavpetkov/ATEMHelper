// LogoControl.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include "LogoClass.h"
#include <thread>
#include <WinSock2.h>

int main()
{
#ifdef _WIN32
	INT rc;
	WSADATA wsaData;

	rc = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (rc) {
		printf("WSAStartup Failed.\n");
		return 1;
	}
#endif

	LogoClass lc("LG-123", "192.168.16.139");

	size_t test = 1;
	while (true)
	{

		lc.EnableLogo(1, (test % 2) + 1);
		lc.EnableLogo(2, (test % 2) + 3);

		test++;

		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}
}
