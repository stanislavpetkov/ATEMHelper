#pragma once

/*
* Add Libs microhttpd;pthread;stdc++fs;rt to your linker	
*/


#ifdef _WEBSOCKETS_
#define  ALLOW_WEBSOCKETS 1
#endif



#ifdef _SSL_

//We have no implementation for wss, yet
#ifndef ALLOW_WEBSOCKETS
#define  ALLOW_SSL 1
#endif
#endif

#ifndef __cplusplus
#error A C++ compiler is required!
#endif 

#include <map>
#include <string>
#include <thread>
#include <atomic>
#include <shared_mutex>

#include "../ThirdParty/nlohmann_json/src/json.hpp"
#include "method_types.h"

extern "C"
{
#define BASE_HTTP_PATH Microhttpd

#ifdef _M_X64
#define HTTP_CPU x86_64
#endif

#ifdef _M_IX86
#define HTTP_CPU x86
#endif

#if _MSC_VER >= 1920
#define VSVER_PATH VS2019
#elif _MSC_VER >= 1910
#define VSVER_PATH "VS2017"
#else
#error "Unsupported Compiler"
#endif

#ifdef _DEBUG
#ifdef _DLL
#define build_type Debug-dll
#define lib_name libmicrohttpd-dll_d.lib
#else
#define build_type Debug-static
#define lib_name libmicrohttpd_d.lib
#endif
#else
#ifdef _DLL
#define build_type Release-dll
#define lib_name libmicrohttpd-dll.lib
#else
#define build_type Release-static
#define lib_name libmicrohttpd.lib
#endif
#endif // DEBUG

#define PPCAT_NX(A, B, C, D, E) ./A/B/C/D/E
#define PPCAT(A, B, C, D, E) PPCAT_NX(A, B, C, D, E)

#define STRINGIZE_NX(A) #A
#define STRINGIZE(A) STRINGIZE_NX(A)

	


#define MHTTP_HEADER_FILE STRINGIZE(PPCAT(BASE_HTTP_PATH ,HTTP_CPU, VSVER_PATH, build_type, microhttpd.h))

#define MHTTP_LIB_FILE STRINGIZE(PPCAT(BASE_HTTP_PATH ,HTTP_CPU, VSVER_PATH, build_type, lib_name))

#include MHTTP_HEADER_FILE
}


namespace API
{
	enum class Err;
}

struct MHD_Daemon;
struct MHD_Connection;
//typedef int MHD_socket;
struct MHD_UpgradeResponseHandle;

namespace HTTP
{

	struct ci_less
	{
		// case-independent (ci) compare_less binary function
		struct nocase_compare
		{
			bool operator() (const unsigned char& c1, const unsigned char& c2) const {
				return tolower(c1) < tolower(c2);
			}
		};
		bool operator() (const std::string & s1, const std::string & s2) const {
			return std::lexicographical_compare
			(s1.begin(), s1.end(),   // source range
				s2.begin(), s2.end(),   // dest range
				nocase_compare());  // comparison
		}
	};

	typedef std::multimap<std::string, std::string, ci_less> mulmap_of_str_ci; //This has case insensitive find


	struct HTTP_REQUEST
	{
		std::string body;
		mulmap_of_str_ci headers;
		mulmap_of_str_ci params;
		mulmap_of_str_ci cookies;
		std::string url;
		std::string method;


		//optional
		nlohmann::json data_from_body;
		HTTP_REQUEST();
	};


	enum Reply_type_t
	{
		rt_Unknown,
		rt_JSON,
		rt_HTML,
		rt_TEXT,
		rt_XML,
		rt_FILE
	};




	struct HTTP_REPLY
	{
		Reply_type_t type;

		//data ... if type is rt_FILE it contains the filename to be served
		std::string message;

		mulmap_of_str_ci headers;

		//if data is not nlohman json object it will be serialized in message
		nlohmann::json data;

		HTTP_REPLY() :data(nullptr), type(rt_Unknown) {};
		void add_header(const std::string & key, const std::string & value);



		void set_error(API::Err errcode, const std::string & ErrorMessage , const std::string & DevMessage);
	};


	
	

	typedef StatusCode(*HTTP_CB_FUNC)(const uint64_t cookie, const HTTP_REQUEST & request, HTTP_REPLY & reply);

	enum opcode_type {
		CONTINUATION = 0x0,
		TEXT_FRAME = 0x1,
		BINARY_FRAME = 0x2,
		CLOSE = 8,
		PING = 9,
		PONG = 0xa,
	};

	typedef StatusCode(*API_CB_FUNC)(const uint64_t cookie, const HTTP_REQUEST & request, HTTP_REPLY & reply);
	class ApiCall_t
	{
	public:
		ApiCall_t() : APIFunc(nullptr), Schema_Req(nullptr), Schema_Resp(nullptr) {};
		API_CB_FUNC APIFunc;
		const nlohmann::json* Schema_Req;
		const nlohmann::json* Schema_Resp;
	};


	using method_string=std::string;
	using url_string=std::string;
	typedef std::map<method_string, ApiCall_t> Api_Endpoint_Map_t;
	typedef std::map<url_string, Api_Endpoint_Map_t> ApiCallsMap_t;

	class MHD_Http_Server
	{
	private:
		MHD_Daemon * daemon;
		uint64_t the_cookie_;

		std::shared_mutex    endpoints_mtx_;
		ApiCallsMap_t endpoints_;

		HTTP_CB_FUNC ext_default_handler_;
		



		std::atomic<bool> exit_;
		static mulmap_of_str_ci get_request_headers(MHD_Connection *connection);
		static mulmap_of_str_ci get_request_params(MHD_Connection * connection);
		static mulmap_of_str_ci get_request_cookie(MHD_Connection * connection);
		static ssize_t get_content_length(const mulmap_of_str_ci & headers);
		static HTTP_REQUEST make_request(MHD_Connection * connection, const char * url, const char * method, std::string * body);
		static int request_handler(void * cls, MHD_Connection * connection, const char * url, const char * method, const char * version, const char * upload_data, size_t * upload_data_size, void ** con_cls);
		static void upgrade_handler(void *cls,
			struct MHD_Connection *connection,
			void *con_cls,
			const char *extra_in,
			size_t extra_in_size,
			MHD_socket sock,
			struct MHD_UpgradeResponseHandle *urh);

		bool rcv_bytes(MHD_socket sock, uint8_t * buffer, size_t bytes_to_read);

		bool get_websocket_message(MHD_socket sock, std::string & message, opcode_type & opcode, bool & FIN);

		void make_websocket_message(opcode_type opcode_, const std::string & src_message, std::string & packet);

		bool websocket_handler(MHD_socket sock, MHD_UpgradeResponseHandle * urh);

		StatusCode router(HTTP_REQUEST & request, HTTP_REPLY & reply);
		StatusCode default_callback(const HTTP_REQUEST & request, HTTP_REPLY & reply);
		MHD_Http_Server() = delete;
		MHD_Http_Server(MHD_Http_Server &) = delete;
	public:

		MHD_Http_Server(const uint16_t port, const std::string & ip, uint64_t cookie);
		~MHD_Http_Server();

		
		void add_endpoint(const std::string & method, const std::string & URL, API_CB_FUNC APIFunc, const nlohmann::json* Schema_Req = nullptr, const nlohmann::json* Schema_Resp = nullptr);
		void remove_endpoint(const std::string & method, const std::string & URL);
		void set_default_handler(HTTP_CB_FUNC cb);
	};

}