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
	LogoClass(const std::string& ip);
	~LogoClass();



	std::string Id() const;
	std::string Title() const;
	std::string Ip() const;
	std::string Model() const;
	size_t NumLogos() const;

	std::vector<bool> GetLogosStatus(const uint32_t pathNo);

	void Ip(const std::string& ipaddr);

	bool EnableLogo(const uint32_t pathNo, const uint32_t logoNo);
};

