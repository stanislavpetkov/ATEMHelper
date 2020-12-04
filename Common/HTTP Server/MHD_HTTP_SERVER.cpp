#include "MHD_HTTP_SERVER.h"

#ifndef WINDOWS
#if _WIN32 || _WIN64
#define WINDOWS
#endif
#endif
#ifdef WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif /* !WIN32_LEAN_AND_MEAN */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>


//#define PERFORMANCE_CHECK
#ifdef PERFORMANCE_CHECK
#include "../Clock/Clock.h"
#include "../Logging/Log.h"
class PerformanceLog
{
private:
	int64_t _startTime;
	int64_t _iterDivider;
	std::string _name;
public:
	PerformanceLog(const std::string& name, int64_t iterDivider = 1) :
		_startTime(Clock::get_time(ClockType_t::HighResolution)),
		_name(name), _iterDivider(iterDivider)
	{};

	~PerformanceLog()
	{
		auto endTime = Clock::get_time(ClockType_t::HighResolution);
		Log::performance(_name, "Execution time total: {}, iteration: {} in 100ns units", endTime - _startTime, (endTime - _startTime) / _iterDivider);

	};
};
#endif


#pragma comment(lib, MHTTP_LIB_FILE)
#pragma comment(lib, "Ws2_32.lib")

#endif /* MHD_WINSOCK_SOCKETS */

#ifdef __linux__

extern "C"
{
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
}
#endif

#include <typeinfo>
#include "../SHA1/sha1.h"
#include "../base64/Base64.h"
#include "../byte_swap.h"
#include "../Logging/Log.h"
#include "../API/APICommon.h"
#include "method_types.h"
#include "../ThirdParty/nlohmann_json/src/json.hpp"
#include "../ThirdParty/JsonSchemaValidator/json-schema.hpp"
#include <chrono>

using namespace std::chrono_literals;

#ifdef __linux__
using snd_rcv_size = size_t;
#else
using snd_rcv_size = int;
#endif

namespace HTTP
{
	HTTP_REQUEST::HTTP_REQUEST() :data_from_body(nullptr) {};

	inline bool iequals(const std::string& ext, const std::string& search)
	{
		auto data = ext;
		std::transform(data.begin(), data.end(), data.begin(), ::tolower);
		return data == search;
	}

	std::string get_mime_type_from_file_reply(const HTTP_REPLY& reply)
	{
		const std::string& file_name = reply.message;


		auto const ext = [&file_name]
		{
			auto const pos = file_name.rfind(".");
			if (pos == std::string::npos)
				return std::string{};
			return file_name.substr(pos);
		}();
		if (iequals(ext, ".htm"))  return "text/html";
		if (iequals(ext, ".html")) return "text/html";
		if (iequals(ext, ".php"))  return "text/html";
		if (iequals(ext, ".css"))  return "text/css";
		if (iequals(ext, ".txt"))  return "text/plain";
		if (iequals(ext, ".js"))   return "application/javascript";
		if (iequals(ext, ".json")) return "application/json";
		if (iequals(ext, ".xml"))  return "application/xml";
		if (iequals(ext, ".swf"))  return "application/x-shockwave-flash";
		if (iequals(ext, ".flv"))  return "video/x-flv";
		if (iequals(ext, ".png"))  return "image/png";
		if (iequals(ext, ".jpe"))  return "image/jpeg";
		if (iequals(ext, ".jpeg")) return "image/jpeg";
		if (iequals(ext, ".jpg"))  return "image/jpeg";
		if (iequals(ext, ".gif"))  return "image/gif";
		if (iequals(ext, ".bmp"))  return "image/bmp";
		if (iequals(ext, ".ico"))  return "image/vnd.microsoft.icon";
		if (iequals(ext, ".tiff")) return "image/tiff";
		if (iequals(ext, ".tif"))  return "image/tiff";
		if (iequals(ext, ".svg"))  return "image/svg+xml";
		if (iequals(ext, ".svgz")) return "image/svg+xml";
		return "application/octet-stream";
	}
	std::string get_mime_type_from_reply(const HTTP_REPLY& reply)
	{
		switch (reply.type)
		{
		case rt_JSON: return "application/json; charset=utf-8";
		case rt_HTML: return "text/html";
		case rt_TEXT: return "text/plain";
		case rt_XML: return "application/xml";
		case rt_FILE: return get_mime_type_from_file_reply(reply);

		default:
			return "application/octet-stream";
		}
	}


	mulmap_of_str_ci MHD_Http_Server::get_request_headers(MHD_Connection* connection)
	{
		mulmap_of_str_ci map;
		MHD_get_connection_values(connection, MHD_ValueKind::MHD_HEADER_KIND,
			[](void* cls,
				enum MHD_ValueKind kind,
				const char* key,
				const char* value)->int {
					if (cls == nullptr) return MHD_NO;

					mulmap_of_str_ci* map = (mulmap_of_str_ci*)cls;
					map->insert(mulmap_of_str_ci::value_type(key, value));
					return MHD_YES;
			}
		, &map);
		return map;
	}

	mulmap_of_str_ci MHD_Http_Server::get_request_params(MHD_Connection* connection)
	{
		mulmap_of_str_ci map;
		MHD_get_connection_values(connection, MHD_ValueKind::MHD_GET_ARGUMENT_KIND,
			[](void* cls,
				enum MHD_ValueKind kind,
				const char* key,
				const char* value)->int {
					if (cls == nullptr) return MHD_NO;

					mulmap_of_str_ci* map = (mulmap_of_str_ci*)cls;
					if (value != nullptr)
					{
						map->insert(mulmap_of_str_ci::value_type(key, value));
					}
					else
					{
						map->insert(mulmap_of_str_ci::value_type(key, ""));
					}
					return MHD_YES;
			}
		, &map);
		return map;
	}


	mulmap_of_str_ci MHD_Http_Server::get_request_cookie(MHD_Connection* connection)
	{
		mulmap_of_str_ci map;
		MHD_get_connection_values(connection, MHD_ValueKind::MHD_COOKIE_KIND,
			[](void* cls,
				enum MHD_ValueKind kind,
				const char* key,
				const char* value)->int {
					if (cls == nullptr) return MHD_NO;

					mulmap_of_str_ci* map = (mulmap_of_str_ci*)cls;
					map->insert(mulmap_of_str_ci::value_type(key, value));
					return MHD_YES;
			}
		, &map);
		return map;
	}




	ssize_t MHD_Http_Server::get_content_length(const mulmap_of_str_ci& headers)
	{
		auto hdr = headers.find("content-length");
		if (hdr == headers.end()) return -1;
		return std::atoll(hdr->second.c_str());
	}

	HTTP_REQUEST MHD_Http_Server::make_request(MHD_Connection* connection, const char* url, const char* method, std::string* body)
	{
		HTTP_REQUEST req;
		req.url = url;
		req.method = method;
		req.cookies = get_request_cookie(connection);
		req.params = get_request_params(connection);
		req.headers = get_request_headers(connection);
		auto content_length = get_content_length(req.headers);
		if ((content_length > 0) && (body != nullptr))
		{
			req.body.swap(*body);
		}
		else
		{
			req.body = {};
		}

		return req;
	}

	MHD_Http_Server::MHD_Http_Server(const uint16_t port, const std::string& ip, uint64_t cookie) :daemon(0), the_cookie_(cookie), ext_default_handler_(nullptr), exit_(false)
	{
		unsigned int flags = MHD_USE_THREAD_PER_CONNECTION | MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_PIPE_FOR_SHUTDOWN;


#if defined(ALLOW_WEBSOCKETS)
		flags |= MHD_ALLOW_UPGRADE;
#endif

#ifdef DEBUG
		flags |= MHD_USE_DEBUG;
#endif


#ifdef ALLOW_SSL
		if (MHD_is_feature_supported(MHD_FEATURE_SSL) == MHD_YES)
		{
			flags |= MHD_USE_SSL;
			Log::debug(__FUNCTION__, "SSL ON");
		}
		else
		{
			Log::debug(__FUNCTION__, "SSL OFF");
		}
#endif

		if (ip.empty())
		{
			daemon = MHD_start_daemon(flags, port,
				nullptr, nullptr,
				&request_handler, (void*)this, nullptr, MHD_OPTION_END);
			if (nullptr == daemon) throw std::runtime_error("MHD Daemon can not be created!!!");

		}
		else
		{
			sockaddr_in daemon_ip_addr{};
			daemon_ip_addr.sin_family = AF_INET;
			daemon_ip_addr.sin_port = htons(port);
			int res = inet_pton(AF_INET, ip.c_str(), &daemon_ip_addr.sin_addr);

			if (res > 0)
			{
				daemon = MHD_start_daemon(flags, port,
					nullptr, nullptr,
					&request_handler, (void*)this, MHD_OPTION_SOCK_ADDR, &daemon_ip_addr, MHD_OPTION_END);
			}
			else
				if (res == 0)
				{
					sockaddr_in6 daemon_ip_addr6{};
					daemon_ip_addr6.sin6_family = AF_INET6;
					daemon_ip_addr6.sin6_port = htons(port);

					res = inet_pton(AF_INET6, ip.c_str(), &daemon_ip_addr6.sin6_addr);

					if (res > 0)
					{
						flags |= MHD_USE_IPv6;
						daemon = MHD_start_daemon(flags, port,
							nullptr, nullptr,
							&request_handler, (void*)this, MHD_OPTION_SOCK_ADDR, &daemon_ip_addr6, MHD_OPTION_END);
					}
					if (res == 0)
					{
						throw std::runtime_error("Invalid IPV6 IP Address");
					}
				}
				else
				{
					throw std::runtime_error("Invalid address");
				}

			if (nullptr == daemon) throw std::runtime_error("MHD Daemon can not be created!!!");
		}
	}


	MHD_Http_Server::~MHD_Http_Server()
	{
		exit_.store(true);

		//while (true)
		//{
		//	const MHD_DaemonInfo * dit = MHD_get_daemon_info(daemon, MHD_DaemonInfoType::MHD_DAEMON_INFO_CURRENT_CONNECTIONS);
		//	if (dit->num_connections == 0) break;			
		//	std::this_thread::sleep_for(10ms);

		//}
		MHD_stop_daemon(daemon);
	}

	void MHD_Http_Server::add_endpoint(const std::string& method, const std::string& URL, API_CB_FUNC APIFunc, const nlohmann::json* Schema_Req, const nlohmann::json* Schema_Resp)
	{
		std::unique_lock<std::shared_mutex>  locker(endpoints_mtx_);

		ApiCall_t api;
		api.APIFunc = APIFunc;
		api.Schema_Req = Schema_Req;
		api.Schema_Resp = Schema_Resp;

		endpoints_[URL][method] = api;
	}

	void MHD_Http_Server::remove_endpoint(const std::string& method, const std::string& URL)
	{


		try
		{
			std::unique_lock<std::shared_mutex>  locker(endpoints_mtx_);
			auto ep = endpoints_.at(URL);
			ep.erase(method);
		}
		catch (...) //std::out_of_range 
		{
			return;
		}
	}

	void MHD_Http_Server::set_default_handler(HTTP_CB_FUNC cb)
	{
		std::unique_lock<std::shared_mutex>  locker(endpoints_mtx_);
		ext_default_handler_ = cb;
	}


	int MHD_Http_Server::request_handler(void* cls, struct MHD_Connection* connection,
		const char* url, const char* method,
		const char* version, const char* upload_data,
		size_t* upload_data_size, void** con_cls)
	{
#ifdef PERFORMANCE_CHECK
		PerformanceLog log(fmt::format("request_handler   {}",url));
#endif
		struct MHD_Response* response;
		auto server = (MHD_Http_Server*)cls;
		int ret = 0;
		std::string s_method(method);
		HTTP_REQUEST request{};


		if ((s_method != methods::s_post_method) &&
			(s_method != methods::s_get_method) &&
			(s_method != methods::s_delete_method) &&
			(s_method != methods::s_put_method))
		{
			auto page = status_code(StatusCode::client_error_method_not_allowed);

			response = MHD_create_response_from_buffer(page.size(), (void*)page.c_str(), MHD_RESPMEM_MUST_COPY);
			ret = MHD_queue_response(connection, MHD_HTTP_METHOD_NOT_ALLOWED, response);


			MHD_destroy_response(response);
			return MHD_YES;
		}





		if ((s_method == methods::s_post_method) || (s_method == methods::s_put_method) || (s_method == methods::s_get_method) || (s_method == methods::s_delete_method))
		{
			if ((upload_data == nullptr) && ((*con_cls) == nullptr))
			{
				auto content_length = get_content_length(get_request_headers(connection));
				auto vec = new std::string();
				if (content_length > 0)
				{
					vec->reserve(static_cast<size_t>(content_length));
				}
				*con_cls = vec;
				return MHD_YES;
			}

			auto vec = (std::string*) * con_cls;


			if ((upload_data != nullptr) && ((*upload_data_size) > 0))
			{

				vec->append(upload_data, *upload_data_size);
				*upload_data_size = 0;
				return MHD_YES;
			}
			else
			{
				if (vec != nullptr)
				{
					//we have the body

					request = make_request(connection, url, method, vec);
					delete vec;
					*con_cls = nullptr;

					if (s_method == methods::s_get_method)
					{
						auto upgrade = request.headers.find("upgrade");
						if (upgrade != request.headers.end())
						{
							if (upgrade->second == "websocket") //case sensitive?
							{
								response = MHD_create_response_for_upgrade(upgrade_handler, server);

								auto accept = request.headers.find("Sec-WebSocket-Key");
								if (accept != request.headers.end())
								{
									///*accept->second*/

									std::string magic_key = accept->second + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
									sha1::SHA1 sha;
									sha.processBytes(magic_key.data(), magic_key.size());
									magic_key.resize(20); //5 x uint32_t
									sha.getDigestBytes((uint8_t*)magic_key.data());



									auto b_k = base64_encode(magic_key);

									MHD_add_response_header(response, MHD_HTTP_HEADER_SEC_WEBSOCKET_ACCEPT, b_k.c_str());


									MHD_add_response_header(response, MHD_HTTP_HEADER_SEC_WEBSOCKET_EXTENSIONS, "");


									//auto proto = request.headers.find(MHD_HTTP_HEADER_SEC_WEBSOCKET_PROTOCOL);
									//if (proto != request.headers.end())
									//{
									//	MHD_add_response_header(response,
									//		MHD_HTTP_HEADER_SEC_WEBSOCKET_PROTOCOL, proto->second.c_str());
									//}

									MHD_add_response_header(response, MHD_HTTP_HEADER_SEC_WEBSOCKET_VERSION, "13");

								}

								MHD_add_response_header(response, MHD_HTTP_HEADER_UPGRADE, "WebSocket");

								ret = MHD_queue_response(connection, MHD_HTTP_SWITCHING_PROTOCOLS, response);
								MHD_destroy_response(response);
								return ret;
							}
						}
					}


				}
			}
		}
		else
		{
			return MHD_NO;
		}

		//server
		//<TODO> Call(Cookie, request, reply);

		HTTP_REPLY reply{};
		StatusCode status_code = StatusCode::server_error_internal_server_error;
		try
		{

			status_code = server->router(request, reply);

		}
		catch (const std::exception& e)
		{
			reply = {};
			reply.message = "Server Error ";
			reply.message.append(" [");
			reply.message.append(e.what());
			reply.message.append("]");


			reply.set_error(API::Err::Exception, "", reply.message);
		}
		catch (...)
		{
			reply = {};
			reply.message = "Server Error [ Unknown Exception ]  ";
			reply.set_error(API::Err::Exception, "", reply.message);
		}



		if (status_code < StatusCode::information_continue)
		{
#ifdef _DEBUG
			//YOUR StatusCode Is Invalid
			__debugbreak();
#endif

			reply = {};
			reply.message = "Server Error [ Endpoint Returned Invalid StatusCode ]  ";
			reply.set_error(API::Err::Exception, "", reply.message);
			status_code = StatusCode::server_error_internal_server_error;
		}



		if (reply.type != rt_FILE)
		{

			if ((reply.type == rt_JSON) && (reply.data.is_object() || reply.data.is_array()))
			{
				reply.message = reply.data.dump();
			}



			response = MHD_create_response_from_buffer(reply.message.size(), (void*)reply.message.c_str(), MHD_RESPMEM_MUST_COPY);
		}
		else
		{
#ifdef WINDOWS
			int fd = -1;
			std::wstring fname = utf8_to_wstring(reply.message);

			if (0 != _wsopen_s(&fd, fname.c_str(), O_RDONLY, _SH_DENYWR, _S_IREAD))
			{
				fd = -1;
			}
#else
			auto fd = open64(reply.message.c_str(), O_RDONLY);
#endif

			if (fd == -1)
			{
				reply.set_error(API::Err::FileNotFound, "", "");
				if ((reply.type == rt_JSON) && (reply.data.is_object()))
				{
					reply.message = reply.data.dump();
				}
				status_code = StatusCode::client_error_not_found;
				response = MHD_create_response_from_buffer(reply.message.size(), (void*)reply.message.c_str(), MHD_RESPMEM_MUST_COPY);
			}
			else
			{

				struct _stat64 st64 {};

#ifdef WINDOWS
				_fstat64(fd, &st64);
#else

				fstat64(fd, &st64);
#endif

				response = MHD_create_response_from_fd64(static_cast<uint64_t>(st64.st_size), fd);
			}

		}




		for (auto kv : reply.headers)
		{
			MHD_add_response_header(response, kv.first.c_str(), kv.second.c_str());
		}
		std::string mime_type = get_mime_type_from_reply(reply);
		MHD_add_response_header(response, "content-type", mime_type.c_str());
#ifdef SERVER_TAG
		MHD_add_response_header(response, "Server", "NEO-API-Server");
#endif

		ret = MHD_queue_response(connection, (unsigned int)status_code, response);
		MHD_destroy_response(response);
		return ret;
	}





	void MHD_Http_Server::upgrade_handler(void* cls, MHD_Connection* connection, void* con_cls,
		const char* extra_in, size_t extra_in_size, MHD_socket sock, MHD_UpgradeResponseHandle* urh)
	{

		auto server = (MHD_Http_Server*)cls;
		server->websocket_handler(sock, urh);
	}

	bool MHD_Http_Server::rcv_bytes(MHD_socket sock, uint8_t* buffer, size_t bytes_to_read)
	{
		size_t buf_pos = 0;
		size_t btr = bytes_to_read;

		while (btr > 0)
		{
			if (exit_) return false;


#ifdef __linux__
			auto bytes = recv(sock, (void*)& buffer[buf_pos], btr, 0);
			if (bytes == 0) return false; //shutdown

			auto serr = errno;
			bool again = serr == EAGAIN;
#else
			auto bytes = recv(sock, (char*)& buffer[buf_pos], (int)btr, 0);
			if (bytes == 0) return false; //shutdown

			auto serr = WSAGetLastError();
			bool again = serr == WSAEWOULDBLOCK;
#endif

			if ((serr != 0) && (!again)) return false;

			if (bytes > 0)
			{
				buf_pos += static_cast<size_t>(bytes);
				btr -= static_cast<size_t>(bytes);
			}
			else
			{
				std::this_thread::sleep_for(2ms);
			}
		}


		return true;
	}






	bool MHD_Http_Server::get_websocket_message(MHD_socket sock, std::string& message, opcode_type& opcode, bool& FIN)
	{
		uint8_t buf[8];
		std::vector<uint8_t> payload;

		if (!rcv_bytes(sock, &buf[0], 1)) return false;
		FIN = (buf[0] & 0x80) == 0x80;
		opcode = (opcode_type)(buf[0] & 0xF);

		if (!rcv_bytes(sock, &buf[0], 1)) return false;



		bool has_mask = (buf[0] & 0x80) == 0x80;

		if (!has_mask) return false; //section 5.1 of the spec says that your server must disconnect from a client if that client sends an unmasked message

		size_t payload_len1 = (buf[0] & 0x7F);
		size_t payload_size = 0;


		if (payload_len1 <= 125)
		{
			payload_size = payload_len1;

		}
		else if (payload_len1 == 126)
		{
			if (!rcv_bytes(sock, &buf[0], 2)) return false;

			uint16_t u16 = static_cast<uint16_t>((((uint16_t)buf[0]) << (uint16_t)8) | static_cast<uint16_t>(buf[1]));
			payload_size = u16;
		}
		else if (payload_len1 == 127)
		{
			if (!rcv_bytes(sock, &buf[0], 8)) return false;


			uint64_t u64 = ((uint64_t)buf[0]) << 56 |
				((uint64_t)buf[1]) << 48 |
				((uint64_t)buf[2]) << 40 |
				((uint64_t)buf[3]) << 32 |
				((uint64_t)buf[4]) << 24 |
				((uint64_t)buf[5]) << 16 |
				((uint64_t)buf[6]) << 8 |
				((uint64_t)buf[7]);
			payload_size = u64;
		}


		uint8_t masking_key[4]{};
		if (has_mask)
		{
			if (!rcv_bytes(sock, &masking_key[0], 4)) return false;
		}


		payload.resize(payload_size);

		if (!rcv_bytes(sock, payload.data(), payload_size)) return false;

		message.resize(payload.size());

		for (size_t i = 0; i < payload.size(); i++) {
			message[i] = static_cast<char>(payload[i] ^ masking_key[i % 4]);
		}
		return true;

	}



	void MHD_Http_Server::make_websocket_message(opcode_type opcode_, const std::string& src_message, std::string& packet)
	{
		constexpr size_t server_mask_size(4);
		constexpr size_t header_min_size(2);
		constexpr size_t u16_size(2);
		constexpr size_t u64_size(8);
		uint8_t opcode = opcode_;
		constexpr uint8_t fin_flag = 0x80;
		constexpr uint8_t mask_flag = 0x00;



		auto src_size = src_message.size();
		uint8_t small_size = 0;

		size_t header_size = header_min_size + server_mask_size; //Server is not masking
		if (src_size <= 125)
		{
			small_size = (uint8_t)src_size;
		}
		else
			if (src_size < INT16_MAX)
			{
				small_size = 126;
				header_size += u16_size;
			}
			else
			{
				small_size = 127;
				header_size += u64_size;
			}

		//calculate dest_packet_size
		size_t dst_packet_size = header_size + src_size;
		packet.resize(dst_packet_size);

		auto base_hdr = (uint8_t*)(packet.data());

		*base_hdr = fin_flag | opcode;
		base_hdr++;
		*base_hdr = mask_flag | small_size;
		base_hdr++;

		if (src_size > 125)
		{
			if (src_size < INT16_MAX)
			{
				*((unsigned short*)base_hdr) = static_cast<unsigned short>(src_size);
				swapByteOrder(*((unsigned short*)base_hdr));
				base_hdr += u16_size;
			}
			else
			{
				*((unsigned long long*)base_hdr) = static_cast<unsigned long long>(src_size);
				swapByteOrder(*((unsigned long long*)base_hdr));
				base_hdr += u64_size;

			}


		}

		for (size_t i = 0; i < src_message.size(); i++) {
			base_hdr[i] = static_cast<uint8_t>(src_message[i]);
		}



	}

	bool send_message(MHD_socket sock, const std::string& message)
	{

		while (true)
		{
			auto res = send(sock, message.data(), static_cast<snd_rcv_size>(message.size()), 0);
			if (res == -1)
			{
#ifdef __linux__
				auto serr = errno;
				bool again = serr == EAGAIN;
#else
				auto serr = WSAGetLastError();
				bool again = serr == WSAEWOULDBLOCK;
#endif
				if ((serr != 0) & (!again)) return false;
				continue;
			}


			return res == (int)message.size();
		}
	}



	bool MHD_Http_Server::websocket_handler(MHD_socket sock, MHD_UpgradeResponseHandle* urh)
	{
		while (!exit_)
		{
			std::string message;
			opcode_type opcode;
			bool FIN;
			if (!get_websocket_message(sock, message, opcode, FIN))
			{
				MHD_upgrade_action(urh, MHD_UPGRADE_ACTION_CLOSE);
				return false;
			}

			if (!FIN)
			{
				Log::error(__FUNCTION__, "FIN Flag is not set!!!");
				MHD_upgrade_action(urh, MHD_UPGRADE_ACTION_CLOSE);
				return false;
			}

			switch (opcode)
			{
			case HTTP::CONTINUATION:
				Log::debug(__FUNCTION__, "CONTINUATION");
				break;
			case HTTP::TEXT_FRAME:
			{

				if (message.size() < 128)
				{
					Log::debug(__FUNCTION__, "TEXT size: {}, text: {}", message.size(), message);
				}
				else
				{
					Log::debug(__FUNCTION__, "TEXT size: {}", message.size());
				}


				std::string packet{};
				std::string the_msg;
				the_msg.resize(700, 'A');


				make_websocket_message(opcode_type::TEXT_FRAME, the_msg, packet);
				if (!send_message(sock, packet))
				{
					MHD_upgrade_action(urh, MHD_UPGRADE_ACTION_CLOSE);
					return false;
				}
				break;
			}


			case HTTP::BINARY_FRAME:
				Log::debug(__FUNCTION__, "BINARY");
				break;
			case HTTP::CLOSE:
			{
				Log::debug(__FUNCTION__, "CLOSE");
				MHD_upgrade_action(urh, MHD_UPGRADE_ACTION_CLOSE);
				return false;
			}
			case HTTP::PING:
			{
				Log::debug(__FUNCTION__, "PING");
				std::string packet{};
				make_websocket_message(opcode_type::PONG, message, packet);

				if (!send_message(sock, packet))
				{
					MHD_upgrade_action(urh, MHD_UPGRADE_ACTION_CLOSE);
					return false;
				}

			}
			break;
			case HTTP::PONG:
				Log::debug(__FUNCTION__, "PONG");
				//Do nothing
				break;
			default:
			{
				Log::debug(__FUNCTION__, "Unknown opcode {}", opcode);
				MHD_upgrade_action(urh, MHD_UPGRADE_ACTION_CLOSE);
				return false;
			}
			}
		}

		return true;
	}


	static inline bool ValidateHTTPData(const nlohmann::json& cJsonData, const nlohmann::json& cSchema, std::string& errorMessage)
	{
		using nlohmann::json_schema_draft4::json_validator;
		json_validator validator;

		try {
			validator.set_root_schema(cSchema);
		}
		catch (const std::exception& e)
		{
			errorMessage = e.what();
			Log::error(__FUNCTION__, "Validation failed, here is why: {}", e.what());
			return false;
		}



		try {
			validator.validate(cJsonData);
		}
		catch (const std::exception& e) {
			errorMessage = e.what();
			Log::error(__FUNCTION__, "Validation failed, here is why: {}", e.what());
			return false;
		}
		errorMessage = "";
		return true;
	}





	StatusCode MHD_Http_Server::router(HTTP_REQUEST& request, HTTP_REPLY& reply)
	{
		//This function have try/catch from outside

		std::shared_lock<std::shared_mutex>  locker(endpoints_mtx_);

		if ((request.url == "/help/api_urls") && (request.method == HTTP::methods::s_get_method))
		{
			reply.data = nlohmann::json::object();
			reply.type = rt_JSON;
			for (auto& it : endpoints_)
			{
				nlohmann::json jsLink;
				jsLink[API::keys::url] = it.first;
				for (auto& it2 : it.second)
				{
					jsLink[API::keys::methods].push_back(it2.first);
				}
				reply.data[API::keys::urls].push_back(jsLink);
			}

			return StatusCode::success_ok;
		}


		if (endpoints_.empty()) return default_callback(request, reply);


		if (std::find(request.url.begin(), request.url.end(), '{') != request.url.end())
		{
			return StatusCode::client_error_bad_request;
		}


		auto url_map = endpoints_.find(request.url);
		if (url_map == endpoints_.end())
		{

			mulmap_of_str_ci localParams;

			std::string rUrl = toUpperString(request.url);

			//for (auto& ep : endpoints_)
			for (auto ep = endpoints_.begin(); ep != endpoints_.end(); ep++)
			{
				auto& uridef = ep->first;
				localParams.clear();
				//request /a/b/c/d
				//endpoint /a/{testid}/{catchme}/d


				auto startIter = uridef.begin();
				auto reqStartIter = request.url.begin();
				char prevSymbol = 0;
				std::string current;
				current.reserve(uridef.size());
				while (startIter != uridef.end())
				{
					if (std::find(startIter, uridef.end(), '{') == uridef.end()) break;

					if (*startIter == '{')
					{
						auto pos = std::find(startIter, uridef.end(), '}');
						if (pos == uridef.end()) break;

						std::string paramName;
						paramName.append(startIter, pos + 1);
						startIter = pos + 1;

						auto reqposIt = std::search(reqStartIter, request.url.end(), std::default_searcher(current.begin(), current.end()));



						if (reqposIt == request.url.end())
						{
							localParams.clear();

							break;
						}
						if (reqposIt != reqStartIter)
						{
							localParams.clear();
							break;
						}

						reqStartIter = reqposIt + current.size();
						if (reqStartIter >= request.url.end()) {
							localParams.clear();
							break;
						}

						auto dstPos = std::find(reqStartIter, request.url.end(), '/');



						std::string value;
						value.append(reqStartIter, dstPos);

						if (!value.empty() && (!paramName.empty()))
						{
							localParams.insert(mulmap_of_str_ci::value_type(paramName, value));
						}
						paramName = "";
						value = "";
						current = "";

						reqStartIter = dstPos;

					}
					else
					{
						current += *startIter;
						startIter++;
					}


				}

				if ((startIter != uridef.end()) && (*startIter == '/')) startIter++;
				if ((startIter != uridef.end()) && (*startIter == '*'))
				{
					reqStartIter = request.url.end();
					startIter = uridef.end();
				}
				if (startIter != uridef.end())
				{

					auto reqposIt = std::search(reqStartIter, request.url.end(), std::default_searcher(startIter, uridef.end()));
					if ((reqposIt != request.url.end()) && (reqposIt + (uridef.end() - startIter) == request.url.end()))
					{
						reqStartIter = request.url.end();
					}

				}
				if (!localParams.empty() && (reqStartIter == request.url.end()))
				{
					url_map = ep;
					request.params.insert(localParams.begin(), localParams.end());
					break;
				}

			}


			if (url_map == endpoints_.end())
			{
				std::string txt = fmt::format("Requested URL: {}, method: {}. URL not Found.", request.url, request.method);
				reply.set_error(API::Err::URL_NotFound, "Requested URL Not Found", txt);
				return StatusCode::client_error_bad_request;
			}
		}

		auto schema_method_it = request.params.find("schema_method");
		if (schema_method_it != request.params.end())
		{
			auto method = schema_method_it->second;
			auto schema_type_it = request.params.find("schema_type");
			if (schema_type_it != request.params.end())
			{
				auto api_call_it = url_map->second.find(method);
				if (api_call_it != url_map->second.end())
				{
					if ((schema_type_it->second == "request") && (api_call_it->second.Schema_Req != nullptr))
					{
						reply.type = Reply_type_t::rt_JSON;
						reply.data = *(api_call_it->second.Schema_Req);
						return StatusCode::success_ok;
					}
					else
						if ((schema_type_it->second == "response") && (api_call_it->second.Schema_Resp != nullptr))
						{
							reply.type = Reply_type_t::rt_JSON;
							reply.data = *(api_call_it->second.Schema_Resp);
							return StatusCode::success_ok;
						}

				}
			}

			reply.set_error(API::Err::URL_NotFound, "Schema not found", "Schema not found");
			return  HTTP::StatusCode::client_error_bad_request;

		}

		auto api_call_it = url_map->second.find(request.method);

		if (api_call_it == url_map->second.end())
		{

			std::string txt = fmt::format("Requested URL: {}, method: {}. Method not supported.", request.url, request.method);
			reply.set_error(API::Err::UnsupportedMethod, "Requested method unsupported", txt);
			return StatusCode::client_error_bad_request;
		}

		auto& apif = api_call_it->second;




		if (apif.APIFunc == nullptr) {

			std::string txt = fmt::format("Requested URL: {}, method: {}. API function undefined!!!", request.url, request.method);
			reply.set_error(API::Err::CallbackNotAssigned, "Callback not assigned", txt);
			return StatusCode::server_error_internal_server_error;
		}


		if (apif.Schema_Req != nullptr)
		{
			try
			{
				request.data_from_body = nlohmann::json::parse(request.body);
				if (!ValidateHTTPData(request.data_from_body, *apif.Schema_Req, reply.message))
				{
					reply.set_error(API::Err::ValidationFailed, "JSON validation failed", reply.message);
					return HTTP::StatusCode::client_error_bad_request;
				}
			}
			catch (const std::exception& e)
			{
				std::string txt = fmt::format("Excpetion in schema validation. {}", e.what());
				reply.set_error(API::Err::CallbackNotAssigned, "EXCEPTION IN SCHEMA VALIDATION", txt);
				return StatusCode::server_error_internal_server_error;
			}
		}

		return api_call_it->second.APIFunc(the_cookie_, request, reply);
	}







	StatusCode MHD_Http_Server::default_callback(const HTTP_REQUEST& request, HTTP_REPLY& reply)
	{
		auto cb = ext_default_handler_;
		if (cb != nullptr)
		{
			return cb(the_cookie_, request, reply);
		}


		reply.set_error(API::Err::NotImplemented, "", fmt::format("Requested URL: {}, method: {}. Not Found.", request.url, request.method));
		return StatusCode::client_error_not_found;
	}
	void HTTP_REPLY::add_header(const std::string& key, const std::string& value)
	{
		headers.insert(mulmap_of_str_ci::value_type(key, value));
	}
	void HTTP_REPLY::set_error(API::Err errcode, const std::string& ErrorMessage, const std::string& DevMessage)
	{

		type = rt_JSON;
		data = nlohmann::json::object();
		data[API::keys::error] = nlohmann::json::object();
		data[API::keys::error][API::keys::error_code] = static_cast<uint32_t>(errcode);

		data[API::keys::error][API::keys::error_message] = ErrorMessage.empty() ? API::get_default_err_text(errcode) : ErrorMessage;
		data[API::keys::error][API::keys::error_dev_message] = DevMessage.empty() ? API::get_default_err_text(errcode) : DevMessage;
		message.clear();
	}

}