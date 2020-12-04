// ATEMHelper.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "ATEMSManager.h"
#include "../LibLogging/LibLogging.h"
int main()
{
    Log::SetAsyncLogging(true);
    Log::SetFileLogging(true);
    Log::SetODSLogging(true);
    Log::SetConsoleLogging(true);
    Log::SetInstanceId("ATEM");
    Log::SetLogLevel(LOGLEVEL::Debug);

    ATEMSManager a;
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::cout << "Hello World!\n";
}
