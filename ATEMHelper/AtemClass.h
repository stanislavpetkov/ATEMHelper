#pragma once
#include <memory>
#include <string>
class AtemClass
{
	struct impl;
private:
	std::unique_ptr<impl> impl_;
public:
	AtemClass(const std::string& ip);
	~AtemClass();
};

