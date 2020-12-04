#pragma once
#include <atlbase.h>
#include <thread>
#include <vector>
#include "AtemClass.h"


class ATEMSManager
{
private:
	std::vector<std::shared_ptr<AtemClass>> atems;
public:
	ATEMSManager();
};

