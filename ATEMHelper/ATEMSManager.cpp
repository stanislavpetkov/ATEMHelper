#include "ATEMSManager.h"





ATEMSManager::ATEMSManager()
{
	atems.push_back(std::make_shared<AtemClass>( "192.168.10.240"));
	
}
