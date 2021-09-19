#pragma once
#include <memory>

class ControllerClass
{
	struct impl;
private:
	std::shared_ptr<impl> impl_;
public:
	ControllerClass();
	~ControllerClass();
};

