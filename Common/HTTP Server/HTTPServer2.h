#pragma once

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0a00
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <http.h>
#include <Winsock2.h>
#include <string>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <memory>
#include <concurrent_queue.h>
#include <map>
#include <vector>
#include <list>
#include <string>
#include <algorithm>
#include <optional>
#include "method_types.h"
#include "../ThirdParty/nlohmann_json/src/json.hpp"
#include "../../Common Defs/ErrorResponse.h"




#pragma comment(lib, "httpapi.lib")
#pragma comment(lib, "Ws2_32.lib")


#define MacroStr(x)   #x
#define MacroStr2(x)  MacroStr(x)
#define Message(desc) __pragma(message(__FILE__ "(" MacroStr2(__LINE__) ") :" #desc))



namespace HTTP
{

	struct HTTP_IO_CONTEXT;
	struct SERVER_CONTEXT;


	

	typedef void(*HTTP_COMPLETION_FUNCTION)(HTTP_IO_CONTEXT*, PTP_IO, ULONG);

	struct HTTP_IO_CONTEXT
	{
		OVERLAPPED Overlapped;
		// Pointer to the completion function
		HTTP_COMPLETION_FUNCTION pfCompletionFunction;
		// Structure associated with the url and server directory
		SERVER_CONTEXT * pServerContext;
	};




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


	struct HTTP_IOREQUEST
	{
		HTTP_IO_CONTEXT __io_context__;
		PHTTP_REQUEST __p_http_request__;
		HTTP_DATA_CHUNK __chunk__;
		std::vector<char> __http_request_buffer__;
		std::vector<char> __body_buffer__;

		mulmap_of_str_ci params;
		mulmap_of_str_ci headers;
		std::string body;

		inline bool getUrlParam(const std::string & paramName, std::string & value) const
		{
			auto itr = params.find(paramName);
			if (itr == params.end()) return false;

			value = itr->second;
			return true;
		}


		std::optional<std::string> getUrlParam(const std::string& paramName) const
		{			
			auto itr = params.find(paramName);
			if (itr == params.end()) return {};			
			return itr->second;
		}

		std::string get_relative_url() const;
		std::string get_body_as_string() const;
		HTTP_VERB method() const;
		std::string method_as_string() const;
		void setParamsAndHeaders();


		HTTP_IOREQUEST();
	private:
		void populate_headers();
		void populate_params();
	};

	enum Reply_type_t
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

		//StatusCode MakeError(StatusCode status, const int ErrCode, const std::string& category, const std::string& message, const std::string& DevMessage, nlohmann::json Data = nullptr);

		//StatusCode MakeError(StatusCode status, const std::error_code& ec, const std::string& DevMessage, nlohmann::json Data = nullptr);
		//StatusCode MakeError(StatusCode status, const std::error_code& ec, const std::string& Message, const std::string& DevMessage, nlohmann::json Data = nullptr);

		//StatusCode StateError(const std::error_code& ec, const std::string& Message, const std::string& DevMessage = "", nlohmann::json Data = nullptr);

		////StatusCode ArgumentError(const std::error_code& ec, const std::string& Message, const std::string& DevMessage = "", nlohmann::json Data = nullptr);

		////StatusCode MakeError(StatusCode status, const std::string& ErrorMessage, const std::string& DevMessage, nlohmann::json Data = nullptr);
		////StatusCode MakeErrorExtra(StatusCode status, int ErrorCode, const std::string& ErrorCategory, const std::string& ErrorMessage, const std::string& DevMessage, nlohmann::json Data);
		//
		

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

	struct HTTP_IORESPONSE
	{
		HTTP_IO_CONTEXT __io_context__;

		// Structure associated with the specific response
		HTTP_RESPONSE http_response;

		// Structure represents an individual block of data either in memory,
		// in a file, or in the HTTP Server API response-fragment cache.
		HTTP_DATA_CHUNK http_data_chunk[64];


		HTTP_REPLY reply;
	};

	
	typedef StatusCode(*HTTP_CALLBACK_FN)(const uint64_t cookie, const HTTP_IOREQUEST& request, HTTP_REPLY& reply);

	class HTTPServer2;

	struct SERVER_CONTEXT
	{
		// Session Id
		HTTP_SERVER_SESSION_ID sessionId;
		// URL group
		HTTP_URL_GROUP_ID urlGroupId;
		// Request queue handle
		HANDLE hRequestQueue;
		// IO object
		PTP_IO Io;
		// TRUE, when the HTTP Server API driver was initialized
		BOOL bHttpInit;
		// TRUE, when we receive a user command to stop the server
		BOOL bStopServer;

		
		HTTPServer2 * svrClass;
	};

	class HTTPServer2
	{
		struct ENDPOINT
		{
			std::string method;
			std::string relative_url;
			HTTP_CALLBACK_FN callbackFn;
		};
	private:
		SERVER_CONTEXT server_context;
		std::string server_name;
		std::string _base_url;

		std::mutex mtx_requests;
		std::map<uint64_t, std::shared_ptr<HTTP_IOREQUEST>> used_http_io_requests;
		concurrency::concurrent_queue<std::shared_ptr<HTTP_IOREQUEST>> free_http_io_requests;


		std::mutex mtx_reply;
		std::map<uint64_t, std::shared_ptr<HTTP_IORESPONSE>> used_http_io_replies;
		concurrency::concurrent_queue<std::shared_ptr<HTTP_IORESPONSE>> free_http_io_replies;
		std::string _fullFsPath;
		std::string _indexFile;

		std::shared_mutex m_configMutex;

		std::list<ENDPOINT> _registered_endpoints;


		std::atomic<bool> m_Initialized;
		uint32_t m_InitializationCode;
		uint64_t cookie;



		std::atomic<bool>  m_Started;

		static HTTP_IOREQUEST * AllocateHttpIoRequest(SERVER_CONTEXT & ServerContext);
		static void CleanupHttpIoRequest(HTTP_IOREQUEST * pIoRequest);

		static HTTP_IORESPONSE * AllocateHttpIoResponse(SERVER_CONTEXT * pServerContext);
		static void CleanupHttpIoResponse(HTTP_IORESPONSE * pIoResponse);



		static bool InitializeHttpServer(SERVER_CONTEXT & pServerContext);
		static bool InitializeServerIo(SERVER_CONTEXT & ServerContext);

		static void UninitializeServerIo(SERVER_CONTEXT & ServerContext);
		static void UninitializeHttpServer(SERVER_CONTEXT & ServerContext);

		bool StartServer(SERVER_CONTEXT & ServerContext);
		static void StopServer(SERVER_CONTEXT & ServerContext);


		static void ReceiveCompletionCallback(HTTP_IO_CONTEXT * IoContext, PTP_IO Io, ULONG IoResult);

		static void PostNewReceive(SERVER_CONTEXT * pServerContext, PTP_IO Io);

		static void SendCompletionCallback(HTTP_IO_CONTEXT * pIoContext, PTP_IO Io, ULONG IoResult);

		static void ProcessReceiveAndPostResponse(HTTP_IOREQUEST * pIoRequest, PTP_IO Io, ULONG IoResult);

		StatusCode Router(HTTP_IOREQUEST& request, HTTP_REPLY& reply);
		bool RemoveRequestURLs();
	public:
		HTTPServer2(const uint64_t Cookie, const std::string & bind_ip, const uint16_t port,    const std::string & ServerName = "API Server - ", bool useHttps = false);
		~HTTPServer2();

		bool Start();


		bool add_endpoint(const std::string& method, const std::string& URL, HTTP_CALLBACK_FN Callback);
		void remove_endpoint(const std::string& method, const std::string& URL);

		void set_document_root(const std::string& fullFsPath, const std::string & indexFileName = "index.html");

	};


}

