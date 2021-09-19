#pragma once
#include <string>
#include <memory>
#include <vector>
class LogoClass
{
	struct impl;
private:
	std::shared_ptr<impl> impl_;
public:
	LogoClass() = delete;
	LogoClass(const LogoClass&) = delete;
	LogoClass(LogoClass&&) = delete;
	LogoClass(const std::string& id, const std::string& ip);
	~LogoClass();



	std::string Id() const;
	std::string Title() const;
	std::string Ip() const;

	void Ip(const std::string& ipaddr);

	bool EnableLogo(const uint32_t pathNo, const uint32_t logoNo);
};

