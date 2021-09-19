#pragma once

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0a00
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif



#include <string>
#include <atomic>

#include <memory>

#include <map>
#include <vector>
#include <string>
#include <algorithm>
#include <optional>
#include "method_types.h"
#include "../ThirdParty/nlohmann_json/src/json.hpp"
#include "../ErrorResponse.h"


namespace HTTP
{

	struct HTTP_IO_CONTEXT;
	
	struct ci_less
	{
		// case-independent (ci) compare_less binary function
		struct nocase_compare
		{
			inline bool operator() (const unsigned char& c1, const unsigned char& c2) const {
				return tolower(c1) < tolower(c2);
			}
		};
		inline bool operator() (const std::string& s1, const std::string& s2) const {
			return std::lexicographical_compare
			(s1.begin(), s1.end(),   // source range
				s2.begin(), s2.end(),   // dest range
				nocase_compare());  // comparison
		}
	};

	using mulmap_of_str_ci =  std::multimap<std::string, std::string, ci_less>; //This has case insensitive find

	enum class Reply_type_t
	{
		rt_Unknown,
		rt_JSON,
		rt_HTML,
		rt_TEXT,
		rt_OCTET,
		rt_XML,
		rt_FILE,
		rt_JPEG,
		rt_CustomMime
	};

	struct HTTP_REPLY
	{
		Reply_type_t type;
		std::string mimeType;

		//data
		std::string message;

		uint32_t expireCacheSec = 0;

		//use it when you want to give custom reason else keep i t empty
		std::string reason;

		mulmap_of_str_ci headers;

		void insert_header(const std::string & key, const std::string & value);

		template <typename T>
		StatusCode DoError(StatusCode status, ErrorResponse<T> err, const std::string & dev_message="", nlohmann::json data = nullptr)
		{
			static_assert(std::is_enum<T>::value, "Must be an enum type");

			type = Reply_type_t::rt_JSON;
			auto result = nlohmann::json::object();
			headers.clear();

			result["error"] = nlohmann::json::object();
			result["error"]["code"] = err.value();
			result["error"]["code_description"] = err.message();
			result["error"]["category"] = err.category();
			result["error"]["message"] = err.description();
			result["error"]["dev_message"] = dev_message.empty() ? err.description() : dev_message;
			result["error"]["data"] = data;
			message = result.dump();
			return status;
		}
		

	};

	struct HTTP_IOREQUEST
	{
		mulmap_of_str_ci params;
		mulmap_of_str_ci headers;
		std::string body;
		std::string relativeUrl;
		std::string method;
		std::string host;
		

		bool getUrlParam(const std::string& paramName, std::string& value) const;
		std::optional<std::string> getUrlParam(const std::string& paramName) const;
		std::string get_relative_url() const;		
	};


	
	//typedef StatusCode(*HTTP_CALLBACK_FN)(const uint64_t cookie, const HTTP_IOREQUEST& request, HTTP_REPLY& reply);
	using HTTP_CALLBACK_FN = std::function<StatusCode(const uint64_t cookie, const HTTP_IOREQUEST& request, HTTP_REPLY& reply)>;

	
	class HTTPServer2
	{
		struct impl;
		
	private:

		std::shared_ptr<impl> impl_;		
	public:
		HTTPServer2(const uint64_t Cookie, const std::string & bind_ip, const uint16_t port,    const std::string & ServerName = "API Server - ", bool useHttps = false);
		~HTTPServer2();

		bool Start();


		bool add_endpoint(const std::string& method, const std::string& URL, HTTP_CALLBACK_FN Callback);
		void remove_endpoint(const std::string& method, const std::string& URL);

		void set_document_root(const std::string& fullFsPath, const std::string & indexFileName = "index.html");

	};


}

