
#define FMT_HEADER_ONLY
#include "HTTPServer2.h"
#include "../StringUtils.h"
#ifndef LOGRECEIVER
#include "../../LibLogging/LibLogging.h"

#else
#include "../Logging/LocalLog.h"
namespace LOG = SysLoc;
#endif

#include <algorithm>
#include <filesystem>

namespace HTTP
{
	constexpr HTTPAPI_VERSION HttpApiVersion = HTTPAPI_VERSION_2;
	// The number of requests for queueing
	constexpr uint32_t OUTSTANDING_REQUESTS = 16;
	// The number of requests per processor
	constexpr uint32_t REQUESTS_PER_PROCESSOR = 4;

	// This is the size of the buffer we provide to store the request.
	// Headers, URL without entity-body, etc will all be stored in this buffer.
	constexpr uint32_t REQUEST_BUFFER_SIZE = 8192;

	const std::string CacheControl_NoCache = "no-cache, no-store, must-revalidate";
	const std::string Pragma = "no-cache";


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
		if (iequals(ext, ".svgz")) return "image/svg+xml";
		if (iequals(ext, ".m3u8")) return "application/vnd.apple.mpegurl";
		if (iequals(ext, ".ts")) return "video/MP2T";
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
		case rt_OCTET:return "application/octet-stream";
		case rt_JPEG:return "image/jpeg";
		case rt_CustomMime:return reply.mimeType;

		default:
			return "application/octet-stream";
		}
	}




	/* ****************************************** */
	/* ********** HTTP_REPLY **************** */
	/* ****************************************** */

	void HTTP_REPLY::insert_header(const std::string& key, const std::string& value)
	{
		for (auto it = headers.begin(), ite = headers.end(); it != ite;)
		{
			if (it->first == key)
				it = headers.erase(it);
			else
				++it;
		}
		headers.insert(std::make_pair(key, value));
	}

	//StatusCode HTTP_REPLY::MakeError(StatusCode status, const int ErrCode, const std::string& category, const std::string& message, const std::string& DevMessage, nlohmann::json Data)
	//{
	//	return MakeErrorExtra(status, ErrCode, category, message, DevMessage, Data);
	//}

	//StatusCode HTTP_REPLY::MakeError(StatusCode status, const std::error_code& ec, const std::string& DevMessage, nlohmann::json Data)
	//{
	//	return MakeErrorExtra(status, ec.value(), ec.category().name(), ec.message(), DevMessage, Data);		
	//}

	//StatusCode HTTP_REPLY::MakeError(StatusCode status, const std::error_code& ec, const std::string& Message, const std::string& DevMessage, nlohmann::json Data)
	//{
	//	return MakeErrorExtra(status, ec.value(), ec.category().name(), Message, DevMessage, Data);
	//}

	//StatusCode HTTP_REPLY::StateError(const std::error_code& ec, const std::string& Message, const std::string& DevMessage, nlohmann::json Data)
	//{	
	//	type = Reply_type_t::rt_JSON;
	//	auto data = nlohmann::json::object();
	//	reason = "";
	//	headers.clear();

	//	data["error"] = nlohmann::json::object();
	//	data["error"]["code"] = ec.value();
	//	data["error"]["code_description"] = ec.message().empty()? status_code(StatusCode::client_error_conflict): ec.message(); 
	//	data["error"]["category"] = ec.category().name();
	//	data["error"]["message"] = Message;
	//	data["error"]["dev_message"] = DevMessage.empty() ? Message.empty()?status_code(StatusCode::client_error_conflict): Message : DevMessage;
	//	data["error"]["data"] = Data;
	//	message = data.dump();
	//	return StatusCode::client_error_conflict;
	//}

	//StatusCode HTTP_REPLY::ArgumentError(const std::error_code& ec, const std::string& Message, const std::string& DevMessage, nlohmann::json Data)
	//{
	//	type = Reply_type_t::rt_JSON;
	//	auto data = nlohmann::json::object();
	//	reason = "";
	//	headers.clear();

	//	data["error"] = nlohmann::json::object();
	//	data["error"]["code"] = ec.value();
	//	data["error"]["code_description"] = ec.message().empty() ? status_code(StatusCode::client_error_conflict) : ec.message();
	//	data["error"]["category"] = ec.category().name();
	//	data["error"]["message"] = Message;
	//	data["error"]["dev_message"] = DevMessage.empty() ? Message.empty() ? status_code(StatusCode::client_error_conflict) : Message : DevMessage;
	//	data["error"]["data"] = Data;
	//	message = data.dump();
	//	return StatusCode::client_error_bad_request;
	//}
	//StatusCode HTTP_REPLY::MakeErrorExtra(StatusCode status, int ErrorCode, const std::string& ErrorCategory, const std::string& ErrorMessage, const std::string& DevMessage, nlohmann::json Data)
	//{
	//	type = Reply_type_t::rt_JSON;
	//	auto data = nlohmann::json::object();
	//	reason = "";
	//	headers.clear();

	//	data["error"] = nlohmann::json::object();
	//	data["error"]["code"] = ErrorCode;
	//	data["error"]["code_description"] = ErrorMessage.empty() ? status_code(status) : ErrorMessage;
	//	data["error"]["category"] = ErrorCategory;
	//	data["error"]["message"] = ErrorMessage.empty() ? status_code(status) : ErrorMessage;
	//	data["error"]["dev_message"] = DevMessage.empty() ? status_code(status) : DevMessage;
	//	data["error"]["data"] = Data;
	//	message = data.dump();
	//	return status;
	//}


	/* ****************************************** */
	/* ********** HTTP_IOREQUEST **************** */
	/* ****************************************** */

	HTTP_IOREQUEST::HTTP_IOREQUEST() :
		__http_request_buffer__(REQUEST_BUFFER_SIZE),
		__body_buffer__(REQUEST_BUFFER_SIZE),
		__chunk__({}),
		__io_context__({})
	{
		__p_http_request__ = (PHTTP_REQUEST)__http_request_buffer__.data();

	}





	std::string HTTP_IOREQUEST::get_relative_url() const
	{
		auto url_start = __p_http_request__->CookedUrl.pAbsPath;
		auto sz = __p_http_request__->CookedUrl.AbsPathLength / sizeof(url_start[0]);
		return wstring_to_utf8(std::wstring(url_start, sz));
	}

	std::string HTTP_IOREQUEST::get_body_as_string() const
	{
		std::string body = "";
		if (__p_http_request__->EntityChunkCount == 0) return body;

		for (size_t i = 0; i < __p_http_request__->EntityChunkCount; i++)
		{
			if (__p_http_request__->pEntityChunks[i].DataChunkType == HttpDataChunkFromMemory)
			{
				body.append((char*)(__p_http_request__->pEntityChunks[i].FromMemory.pBuffer), __p_http_request__->pEntityChunks[i].FromMemory.BufferLength);
			}

		}
		return body;
	}




	HTTP_VERB HTTP_IOREQUEST::method() const
	{
		return __p_http_request__->Verb;
	}


	std::string HTTP_IOREQUEST::method_as_string() const
	{

		switch (__p_http_request__->Verb)
		{
		case HttpVerbUnparsed: return "UNPARSED";
		case HttpVerbUnknown: return "UNKNOWN";
		case HttpVerbInvalid: return "INVALID";
		case HttpVerbOPTIONS: return methods::s_options_method;
		case HttpVerbGET: return methods::s_get_method;
		case HttpVerbHEAD:return methods::s_head_method;
		case HttpVerbPOST:return methods::s_post_method;
		case HttpVerbPUT:return methods::s_put_method;
		case HttpVerbDELETE:return methods::s_delete_method;
		case HttpVerbTRACE:return methods::s_trace_method;
		case HttpVerbCONNECT:return methods::s_connect_method;
		case HttpVerbTRACK:return methods::s_track_method;
		case HttpVerbMOVE:return methods::s_move_method;
		case HttpVerbCOPY:return methods::s_copy_method;
		case HttpVerbPROPFIND:return methods::s_propfind_method;
		case HttpVerbPROPPATCH:return methods::s_proppatch_method;
		case HttpVerbMKCOL: return methods::s_mkcol_method;
		case HttpVerbLOCK: return methods::s_lock_method;
		case HttpVerbUNLOCK: return methods::s_unlock_method;
		case HttpVerbSEARCH: return methods::s_search_method;
		default:
			break;
		}
		return "UNKNOWN";
	}

	void HTTP_IOREQUEST::setParamsAndHeaders()
	{
		populate_headers();
		populate_params();
		body = get_body_as_string();
	}


	static std::string request_header_name(HTTP_HEADER_ID id)
	{
		switch (id)
		{
		case HttpHeaderCacheControl: return "Cache-Control";
		case HttpHeaderConnection: return "Connection";
		case HttpHeaderDate: return "Date";
		case HttpHeaderKeepAlive: return "Keep-Alive";
		case HttpHeaderPragma: return "Pragma";
		case HttpHeaderTrailer: return "Trailer";
		case HttpHeaderTransferEncoding: return "Transfer-Encoding";
		case HttpHeaderUpgrade: return "Upgrade";
		case HttpHeaderVia: return "Via";
		case HttpHeaderWarning: return "Warning";

		case HttpHeaderAllow: return "Allow";
		case HttpHeaderContentLength: return "Content-Length";
		case HttpHeaderContentType: return "Content-Type";
		case HttpHeaderContentEncoding: return "Content-Encoding";
		case HttpHeaderContentLanguage: return "Content-Language";
		case HttpHeaderContentLocation: return "Content-Location";
		case HttpHeaderContentMd5: return "Content-MD5";
		case HttpHeaderContentRange: return "Content-Range";
		case HttpHeaderExpires: return "Expires";
		case HttpHeaderLastModified: return "Last-Modified";


		case HttpHeaderAccept: return "Accept";
		case HttpHeaderAcceptCharset: return "Accept-Charset";
		case HttpHeaderAcceptEncoding: return "Accept-Encoding";
		case HttpHeaderAcceptLanguage: return "Accept-Language";
		case HttpHeaderAuthorization: return "Authorization";
		case HttpHeaderCookie: return "Cookie";
		case HttpHeaderExpect: return "Expect";
		case HttpHeaderFrom: return "From";
		case HttpHeaderHost: return "Host";
		case HttpHeaderIfMatch: return "If-Match";

		case HttpHeaderIfModifiedSince: return "If-Modified-Since";
		case HttpHeaderIfNoneMatch: return "If-None-Match";
		case HttpHeaderIfRange: return "If-Range";
		case HttpHeaderIfUnmodifiedSince: return "If-Unmodified-Since";
		case HttpHeaderMaxForwards: return "Max-Forwards";
		case HttpHeaderProxyAuthorization: return "Proxy-Authorization";
		case HttpHeaderReferer: return "Referer";
		case HttpHeaderRange: return "Range";
		case HttpHeaderTe: return "TE";
		case HttpHeaderTranslate: return "Translate";

		case HttpHeaderUserAgent: return "User-Agent";
		default:
			break;
		}

		return "Unknown!!!";
	}


	void HTTP_IOREQUEST::populate_headers()
	{
		headers.clear();
		for (size_t i = 0; i < HttpHeaderRequestMaximum; i++)
		{
			if (__p_http_request__->Headers.KnownHeaders[i].RawValueLength > 0)
			{
				std::string value(__p_http_request__->Headers.KnownHeaders[i].pRawValue, __p_http_request__->Headers.KnownHeaders[i].RawValueLength);
				headers.insert(std::make_pair(request_header_name((HTTP_HEADER_ID)i), value));
			}

		}


		for (size_t i = 0; i < __p_http_request__->Headers.UnknownHeaderCount; i++)
		{
			std::string name(__p_http_request__->Headers.pUnknownHeaders[i].pName, __p_http_request__->Headers.pUnknownHeaders[i].NameLength);
			std::string value(__p_http_request__->Headers.pUnknownHeaders[i].pRawValue, __p_http_request__->Headers.pUnknownHeaders[i].RawValueLength);
			headers.insert(std::make_pair(name, value));
		}



	}
	template <class Fn> void split(const char* b, const char* e, char d, Fn fn) {
		int i = 0;
		int beg = 0;

		while (e ? ((b + i) < e) : (b[i] != '\0')) {
			if (b[i] == d) {
				fn(&b[beg], &b[i]);
				beg = i + 1;
			}
			else
				if (b[i] == '\0')
					break;
			i++;
		}

		if (i) { fn(&b[beg], &b[i]); }
	}


	template <class Fn> void split(const std::string::iterator& b, const std::string::iterator& end, char d, Fn fn) {
		std::string::iterator begin = b;
		std::string::iterator current = b;



		while (current < end)
		{
			if (*current == d) {
				fn(begin, current);
				begin = current + 1;
				if (begin == end) break;
			}
			current++;
		}

		if (begin < current) { fn(begin, current); }
	}

	template <class Fn> void split(const std::string::const_iterator& b, const std::string::const_iterator& end, char d, Fn fn) {
		std::string::const_iterator begin = b;
		std::string::const_iterator current = b;



		while (current < end)
		{
			if (*current == d) {
				fn(begin, current);
				begin = current + 1;
				if (begin == end) break;
			}
			current++;
		}

		if (begin < current) { fn(begin, current); }
	}

	inline bool is_hex(char c, int& v) {
		if (0x20 <= c && isdigit(c)) {
			v = c - '0';
			return true;
		}
		else if ('A' <= c && c <= 'F') {
			v = c - 'A' + 10;
			return true;
		}
		else if ('a' <= c && c <= 'f') {
			v = c - 'a' + 10;
			return true;
		}
		return false;
	}

	inline bool from_hex_to_i(const std::string& s, size_t i, size_t cnt,
		int& val) {
		if (i >= s.size()) { return false; }

		val = 0;
		for (; cnt; i++, cnt--) {
			if (!s[i]) { return false; }
			int v = 0;
			if (is_hex(s[i], v)) {
				val = val * 16 + v;
			}
			else {
				return false;
			}
		}
		return true;
	}


	inline size_t to_utf8(int code, char* buff) {
		if (code < 0x0080) {
			buff[0] = (code & 0x7F);
			return 1;
		}
		else if (code < 0x0800) {
			buff[0] = (0xC0 | ((code >> 6) & 0x1F));
			buff[1] = (0x80 | (code & 0x3F));
			return 2;
		}
		else if (code < 0xD800) {
			buff[0] = (0xE0 | ((code >> 12) & 0xF));
			buff[1] = (0x80 | ((code >> 6) & 0x3F));
			buff[2] = (0x80 | (code & 0x3F));
			return 3;
		}
		else if (code < 0xE000) { // D800 - DFFF is invalid...
			return 0;
		}
		else if (code < 0x10000) {
			buff[0] = (0xE0 | ((code >> 12) & 0xF));
			buff[1] = (0x80 | ((code >> 6) & 0x3F));
			buff[2] = (0x80 | (code & 0x3F));
			return 3;
		}
		else if (code < 0x110000) {
			buff[0] = (0xF0 | ((code >> 18) & 0x7));
			buff[1] = (0x80 | ((code >> 12) & 0x3F));
			buff[2] = (0x80 | ((code >> 6) & 0x3F));
			buff[3] = (0x80 | (code & 0x3F));
			return 4;
		}

		// NOTREACHED
		return 0;
	}

	inline std::string decode_url(const std::string& s) {
		std::string result;
		if (s.empty()) return "";

		for (size_t i = 0; i < s.size(); i++) {
			if (s[i] == '%' && i + 1 < s.size()) {
				if (s[i + 1] == 'u') {
					int val = 0;
					if (from_hex_to_i(s, i + 2, 4, val)) {
						if (val == 0) return result;
						// 4 digits Unicode codes
						char buff[4];
						size_t len = to_utf8(val, buff);
						if (len > 0) { result.append(buff, len); }
						i += 5; // 'u0000'
					}
					else {
						result += s[i];
					}
				}
				else {
					int val = 0;
					if (from_hex_to_i(s, i + 1, 2, val)) {
						// 2 digits hex codes
						if (val == 0) return result;
						result += val;
						i += 2; // '00'
					}
					else {
						result += s[i];
					}
				}
			}
			else if (s[i] == '+') {
				result += ' ';
			}
			else {
				if (s[i] == 0) return result;
				result += s[i];
			}
		}

		return result;
	}

	inline void parse_query_text(const std::string& s, mulmap_of_str_ci& params) {
		split(&s[0], &s[s.size()], '&', [&](const char* b, const char* e) {
			std::string key;
			std::string val;
			split(b, e, '=', [&](const char* b, const char* e) {
				if (key.empty()) {
					key.assign(b, e);
				}
				else {
					val.assign(b, e);
				}
				});
			params.emplace(key, decode_url(val));
			});
	}



	void HTTP_IOREQUEST::populate_params()
	{
		params.clear();
		if (__p_http_request__->CookedUrl.QueryStringLength < 1) return;
		if (__p_http_request__->CookedUrl.pQueryString == nullptr) return;

		std::wstring q(__p_http_request__->CookedUrl.pQueryString + 1, __p_http_request__->CookedUrl.QueryStringLength - 1);
		if (q.empty()) return;
		std::string _query = wstring_to_utf8(q);

		parse_query_text(_query, params);


		//std::string::const_iterator it(_query.begin());
		//std::string::const_iterator end(_query.end());
		//while (it != end)
		//{
		//	std::string name;
		//	std::string value;
		//	while (it != end && *it != '=' && *it != '&')
		//	{
		//		if (*it == '+')
		//			name += ' ';
		//		else
		//			name += *it;
		//		++it;
		//	}
		//	if (it != end && *it == '=')
		//	{
		//		++it;
		//		while (it != end && *it != '&')
		//		{
		//			if (*it == '+')
		//				value += ' ';
		//			else
		//				value += *it;
		//			++it;
		//		}
		//	}
		//	std::string decodedName;
		//	std::string decodedValue;
		//	if (!decode(name, decodedName) || !decode(value, decodedValue))
		//	{
		//		return;
		//	}
		//	params.insert(std::make_pair(decodedName, decodedValue));

		//	if (it != end && *it == '&') ++it;
		//}
	}

	/* ****************************************** */
	/* ************* HTTPServer2 **************** */
	/* ****************************************** */




	/* ****************************************** */
	/* ******** Request Allocate/Cleanup ******** */
	/* ****************************************** */

	HTTP_IOREQUEST* HTTPServer2::AllocateHttpIoRequest(
		SERVER_CONTEXT& ServerContext
	)
	{
		HTTPServer2* svr = ServerContext.svrClass;

		std::shared_ptr<HTTP_IOREQUEST> pIoRequest;


		{
			if (!svr->free_http_io_requests.try_pop(pIoRequest))
			{
				pIoRequest = std::make_shared<HTTP_IOREQUEST>();
			}




			if (pIoRequest == nullptr) return nullptr;

			uint64_t memPtr = (uint64_t)pIoRequest.get();
			{
				std::lock_guard<std::mutex> lockit(svr->mtx_requests);
				ServerContext.svrClass->used_http_io_requests.insert(std::pair<uint64_t, std::shared_ptr<HTTP_IOREQUEST>>(memPtr, pIoRequest));
			}
		}


		ZeroMemory(pIoRequest->__http_request_buffer__.data(), pIoRequest->__http_request_buffer__.size());
		ZeroMemory(&(pIoRequest->__io_context__), sizeof(pIoRequest->__io_context__));


		pIoRequest->__io_context__.pServerContext = &ServerContext;
		pIoRequest->__io_context__.pfCompletionFunction = &ReceiveCompletionCallback;
		pIoRequest->__p_http_request__ = (PHTTP_REQUEST)pIoRequest->__http_request_buffer__.data();


		return pIoRequest.get(); //the share_ptr owner is used_http_io_requests so this should be safe
	}

	void HTTPServer2::CleanupHttpIoRequest(HTTP_IOREQUEST* pIoRequest)
	{
		HTTPServer2* svr = pIoRequest->__io_context__.pServerContext->svrClass;
		std::lock_guard<std::mutex> lockit(svr->mtx_requests);

		uint64_t memPtr = (uint64_t)pIoRequest;

		auto itr = svr->used_http_io_requests.find(memPtr);

		if (itr != svr->used_http_io_requests.end())
		{
			svr->free_http_io_requests.push(itr->second);
			svr->used_http_io_requests.erase(memPtr);
		}
	}


	/* ****************************************** */
	/* ******* Response Allocate/Cleanup ******** */
	/* ****************************************** */


	HTTP_IORESPONSE* HTTPServer2::AllocateHttpIoResponse(SERVER_CONTEXT* pServerContext)
	{


		HTTPServer2* svr = pServerContext->svrClass;

		std::shared_ptr<HTTP_IORESPONSE> pIoResponse;


		if (!svr->free_http_io_replies.try_pop(pIoResponse))
		{
			pIoResponse = std::make_shared<HTTP_IORESPONSE>();
		}

		if (pIoResponse == nullptr) return nullptr;

		uint64_t memPtr = (uint64_t)pIoResponse.get();


		{
			std::lock_guard<std::mutex> lockit(svr->mtx_reply);
			svr->used_http_io_replies.insert(std::pair<uint64_t, std::shared_ptr<HTTP_IORESPONSE>>(memPtr, pIoResponse));
		}



		ZeroMemory(&(pIoResponse->__io_context__), sizeof(pIoResponse->__io_context__));
		ZeroMemory(&(pIoResponse->http_data_chunk), sizeof(pIoResponse->http_data_chunk));
		ZeroMemory(&(pIoResponse->http_response), sizeof(pIoResponse->http_response));
		pIoResponse->reply.message = "";
		pIoResponse->reply.type = rt_Unknown;
		pIoResponse->reply.reason = "";
		pIoResponse->reply.headers = {};



		pIoResponse->__io_context__.pServerContext = pServerContext;
		pIoResponse->__io_context__.pfCompletionFunction = SendCompletionCallback;

		pIoResponse->http_response.EntityChunkCount = 0;
		pIoResponse->http_response.pEntityChunks = pIoResponse->http_data_chunk;



		pIoResponse->http_response.Headers.KnownHeaders[HttpHeaderServer].pRawValue = svr->server_name.c_str();
		pIoResponse->http_response.Headers.KnownHeaders[HttpHeaderServer].RawValueLength = (USHORT)svr->server_name.length();


		return pIoResponse.get(); //shared ptr is in used_ entries so this is safe
	}




	void HTTPServer2::CleanupHttpIoResponse(HTTP_IORESPONSE* pIoResponse)
	{



		HTTPServer2* svr = pIoResponse->__io_context__.pServerContext->svrClass;
		std::lock_guard<std::mutex> lockit(svr->mtx_reply);

		uint64_t memPtr = (uint64_t)pIoResponse;

		auto itr = svr->used_http_io_replies.find(memPtr);

		if (itr != svr->used_http_io_replies.end())
		{
			std::shared_ptr<HTTP_IORESPONSE> ptr = itr->second;
			svr->used_http_io_replies.erase(memPtr);
			svr->free_http_io_replies.push(ptr);
		}
	}


	HTTPServer2::HTTPServer2(const uint64_t Cookie, const std::string& bind_ip, const uint16_t port, const std::string& ServerName, bool useHttps) :
		server_context({}),
		m_Started(false),
		server_name(ServerName),
		cookie(Cookie),
		_fullFsPath(""),
		_indexFile("index.html")
	{
		if (useHttps)
		{
			_base_url = "https://";
		}
		else
		{
			_base_url = "http://";
		}

		if (bind_ip == "0.0.0.0")
		{
			_base_url.append("*");
		}
		else
		{
			_base_url.append(bind_ip);
		}
		_base_url.append(":");
		_base_url.append(std::to_string(port));


		server_context.svrClass = this;
		if (!InitializeHttpServer(server_context)) goto CleanServer;
		if (!InitializeServerIo(server_context)) goto CleanIo;

		m_InitializationCode = 0;
		m_Initialized.store(m_InitializationCode == NOERROR);
		if (m_Initialized)
		{
			auto url = _base_url + "/";
			auto ws = utf8_to_wstring(url);

			ULONG ulResult = HttpAddUrlToUrlGroup(
				server_context.urlGroupId,
				ws.c_str(),
				(HTTP_URL_CONTEXT)NULL,
				0);

			if (ulResult != NOERROR)
			{

				if (ulResult == ERROR_ALREADY_EXISTS)
				{
					Log::error(__FUNCTION__, "Error Already Exist {:#x} received in Server Init.Url is /", ulResult);
				}
				else
					if (ulResult == ERROR_ACCESS_DENIED)
					{
						Log::error(__FUNCTION__, "Error Access denied {:#x} received in Server Init.Url is /", ulResult);
					}
					else
						if (ulResult == ERROR_INVALID_PARAMETER)
						{
							Log::error(__FUNCTION__, "Error invalid param {:#x} received in Server Init.Url is /", ulResult);
						}
						else
						{
							Log::error(__FUNCTION__, "Error {:#x} received in Server Init.Url is /", ulResult);
						}
				m_Initialized.store(false);
				UninitializeServerIo(server_context);
				UninitializeHttpServer(server_context);

				throw std::logic_error("Server Can not bind twice");
			}


		}
		return;


	CleanIo:
		UninitializeServerIo(server_context);
	CleanServer:
		UninitializeHttpServer(server_context);


	}


	void HTTPServer2::StopServer(
		SERVER_CONTEXT& ServerContext
	)
	{
		if (ServerContext.hRequestQueue != NULL)
		{
			ServerContext.bStopServer = TRUE;

			HttpShutdownRequestQueue(ServerContext.hRequestQueue);
		}

		if (ServerContext.Io != NULL)
		{
			//
			// This call will block until all IO complete their callbacks.
			WaitForThreadpoolIoCallbacks(ServerContext.Io, true);
		}
	}


	void HTTPServer2::UninitializeServerIo(
		SERVER_CONTEXT& ServerContext
	)
	{
		if (ServerContext.hRequestQueue != NULL)
		{
			HttpCloseRequestQueue(ServerContext.hRequestQueue);
			ServerContext.hRequestQueue = NULL;
		}

		if (ServerContext.Io != NULL)
		{
			CloseThreadpoolIo(ServerContext.Io);
			ServerContext.Io = NULL;
		}
	}

	void HTTPServer2::UninitializeHttpServer(
		SERVER_CONTEXT& ServerContext
	)
	{
		if (ServerContext.urlGroupId != 0)
		{
			HttpCloseUrlGroup(ServerContext.urlGroupId);
			ServerContext.urlGroupId = 0;
		}

		if (ServerContext.sessionId != 0)
		{
			HttpCloseServerSession(ServerContext.sessionId);
			ServerContext.sessionId = 0;
		}

		if (ServerContext.bHttpInit == TRUE)
		{
			HttpTerminate(HTTP_INITIALIZE_SERVER, NULL);
			ServerContext.bHttpInit = FALSE;
		}
	}

	HTTPServer2::~HTTPServer2()
	{
		StopServer(server_context);
		RemoveRequestURLs();
		UninitializeServerIo(server_context);
		UninitializeHttpServer(server_context);
	}



	bool HTTPServer2::InitializeHttpServer(
		SERVER_CONTEXT& ServerContext
	)
	{


		ULONG ulResult = HttpInitialize(
			HttpApiVersion,
			HTTP_INITIALIZE_SERVER,
			NULL);

		if (ulResult != NO_ERROR)
		{
			Log::error(__FUNCTION__, "HttpInitialized failed");
			return false;
		}

		ServerContext.bHttpInit = true;

		ulResult = HttpCreateServerSession(
			HttpApiVersion,
			&(ServerContext.sessionId),
			0);

		if (ulResult != NO_ERROR)
		{
			Log::error(__FUNCTION__, "HttpCreateServerSession failed");
			return false;
		}

		HTTP_PROPERTY_FLAGS propFlags;
		propFlags.Present = 1;
		HTTP_TIMEOUT_LIMIT_INFO timeouts;

		//timeouts in seconds
		//https://msdn.microsoft.com/en-us/library/windows/desktop/aa364661(v=vs.85).aspx
		timeouts.Flags.Present = 1;
		timeouts.EntityBody = 10;
		timeouts.DrainEntityBody = 30;
		timeouts.RequestQueue = 30; //The time, in seconds, allowed for the request to remain in the request queue before the application picks it up.
		timeouts.IdleConnection = 60; //he time, in seconds, allowed for an idle connection.
		timeouts.HeaderWait = 10; //The time, in seconds, allowed for the HTTP Server API to parse the request header.
		timeouts.MinSendRate = 1024; //The minimum send rate, in bytes-per-second, for the response. The default response send rate is 150 bytes-per-second.

		ulResult = HttpSetServerSessionProperty(ServerContext.sessionId,
			HttpServerTimeoutsProperty,
			(PVOID)&timeouts,
			sizeof(timeouts)
		);


		if (ulResult != NO_ERROR)
		{
			Log::error(__FUNCTION__, "HttpSetServerSessionProperty HttpServerTimeoutsProperty  failed");
			return false;
		}



		//HTTP_SERVER_AUTHENTICATION_INFO authInfo;
		//authInfo.Flags.Present = 1;
		//authInfo.AuthSchemes = HTTP_AUTH_ENABLE_BASIC;
		//authInfo.ReceiveMutualAuth = true;
		//authInfo.ReceiveContextHandle = true;
		//authInfo.DisableNTLMCredentialCaching = false;
		//authInfo.ExFlags = 0;
		//authInfo.DigestParams = {};
		////authInfo.BasicParams = {};
		//authInfo.BasicParams.Realm = L"PBT";
		//authInfo.BasicParams.RealmLength = 6;




		//ulResult = HttpSetServerSessionProperty(ServerContext.sessionId,
		//	HttpServerAuthenticationProperty,
		//	(PVOID)&authInfo,
		//	sizeof(authInfo)
		//);


		//if (ulResult != NO_ERROR)
		//{
		//		LOG::error(__FUNCTION__, "HttpSetServerSessionProperty HttpServerTimeoutsProperty  failed");
		//	return false;
		//}


		ulResult = HttpCreateUrlGroup(
			ServerContext.sessionId,
			&(ServerContext.urlGroupId),
			0);

		if (ulResult != NO_ERROR)
		{
			Log::error(__FUNCTION__, "HttpCreateUrlGroup failed");
			return false;
		}


		return true;
	}


	void HTTPServer2::ReceiveCompletionCallback(
		HTTP_IO_CONTEXT* IoContext,
		PTP_IO Io,
		ULONG IoResult
	)
	{
		HTTP_IOREQUEST* pIoRequest;
		SERVER_CONTEXT* pServerContext;


		pIoRequest = CONTAINING_RECORD(IoContext,
			HTTP_IOREQUEST,
			__io_context__);

		pServerContext = pIoRequest->__io_context__.pServerContext;

		if (pServerContext->bStopServer == FALSE)
		{
			ProcessReceiveAndPostResponse(pIoRequest, Io, IoResult);

			PostNewReceive(pServerContext, Io);
		}

		CleanupHttpIoRequest(pIoRequest);
	}


	inline void getRanges(const std::string& rangeHeaderValue, std::vector < std::pair<uint64_t, uint64_t>>& ranges)
	{

		ranges.clear();
		if (rangeHeaderValue.empty()) return;

		std::string key;
		std::string val;
		split(rangeHeaderValue.begin(), rangeHeaderValue.end(), '=', [&](const std::string::const_iterator& b, const std::string::const_iterator& e) {
			if (key.empty()) {
				key.assign(b, e);
				key = toLower(key);
			}
			else {
				val.assign(b, e);
			}
			});
		if (key != "bytes") return;

		split(val.begin(), val.end(), ',', [&](const std::string::iterator& b, const std::string::iterator& e) {
			std::string r;
			r.assign(b, e);
			r = trim(r);
			if (r.empty()) return;

			if (r.front() == '-') // TODO -> this is wrong, but will be fixed some other day
			{
				r = "0" + r;
			}
			if (r.back() == '-') r += std::to_string(HTTP_BYTE_RANGE_TO_EOF);

			std::string first;
			std::string second;
			split(r.begin(), r.end(), '-', [&](const std::string::iterator& b, const std::string::iterator& e) {
				if (first.empty())
				{
					first.assign(b, e);
				}
				else if (second.empty())
				{
					second.assign(b, e);
					uint64_t startPos = atoll(first.c_str());
					uint64_t endPos = atoll(second.c_str());
					ranges.push_back(std::make_pair(startPos, endPos));
				}


				});
			});

		/*startFilePos = 10;
		endFilePos = 30;
*/

	}


	void HTTPServer2::PostNewReceive(
		SERVER_CONTEXT* pServerContext,
		PTP_IO Io
	)
	{
		HTTP_IOREQUEST* pIoRequest;
		ULONG Result;

		pIoRequest = AllocateHttpIoRequest(*pServerContext);

		if (pIoRequest == NULL)
			return;

		StartThreadpoolIo(Io);

		Result = HttpReceiveHttpRequest(
			pServerContext->hRequestQueue,
			HTTP_NULL_ID,
			0,//Headers only -> HTTP_RECEIVE_REQUEST_FLAG_COPY_BODY,
			pIoRequest->__p_http_request__,
			(unsigned long)pIoRequest->__http_request_buffer__.size(),
			NULL,
			&pIoRequest->__io_context__.Overlapped
		);

		if (Result != ERROR_IO_PENDING &&
			Result != NO_ERROR)
		{
			CancelThreadpoolIo(Io);

			Log::error(__FUNCTION__, "HttpReceiveHttpRequest failed, error {:#x}", Result);


			if (Result == ERROR_MORE_DATA)
			{
				ProcessReceiveAndPostResponse(pIoRequest, Io, ERROR_MORE_DATA);
			}

			CleanupHttpIoRequest(pIoRequest);
		}
	}




	void HTTPServer2::SendCompletionCallback(
		HTTP_IO_CONTEXT* pIoContext,
		PTP_IO Io,
		ULONG IoResult
	)
	{
		HTTP_IORESPONSE* pIoResponse;

		UNREFERENCED_PARAMETER(IoResult);
		UNREFERENCED_PARAMETER(Io);

		pIoResponse = CONTAINING_RECORD(pIoContext,
			HTTP_IORESPONSE,
			__io_context__);

		CleanupHttpIoResponse(pIoResponse);
	}

	bool PrepareFileResponse(const std::string& fileName, const std::vector < std::pair<uint64_t, uint64_t>>& ranges, HTTP_IORESPONSE* response, std::string& contentRange)
	{
		std::wstring fn = utf8_to_wstring(fileName);
		response->reply.reason.clear();
		auto FileH = CreateFileW(fn.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
		if (FileH == INVALID_HANDLE_VALUE)
		{
			auto last_error = GetLastError();
			if (last_error == ERROR_FILE_NOT_FOUND)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
				auto FileH = CreateFileW(fn.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);

			}
			else
			{
				Log::info(__func__, "Can't open file error {}", last_error);
			}
			if (FileH == INVALID_HANDLE_VALUE)
			{
				response->http_response.EntityChunkCount = 0;
				response->http_response.StatusCode = (USHORT)StatusCode::client_error_not_found;

				return false;
			}
		}


		BY_HANDLE_FILE_INFORMATION fi{};
		GetFileInformationByHandle(FileH, &fi);
		ULARGE_INTEGER ul;
		ul.HighPart = fi.nFileSizeHigh;
		ul.LowPart = fi.nFileSizeLow;

		response->http_response.EntityChunkCount = 0;
		for (auto& range : ranges)
		{
			uint64_t start = range.first;
			uint64_t end = range.second;
			if (HTTP_BYTE_RANGE_TO_EOF != end)
			{
				if (end > ul.QuadPart) end = HTTP_BYTE_RANGE_TO_EOF;
				//if (end == HTTP_BYTE_RANGE_TO_EOF) end = ul.QuadPart;
				if ((start >= end) || (start >= ul.QuadPart))
				{
					response->http_response.EntityChunkCount = 0;
					response->http_response.StatusCode = (USHORT)StatusCode::client_error_range_not_satisfiable;
					CloseHandle(FileH);
					return false;
				}
			}
			response->http_response.StatusCode = ((start == 0) && (end == HTTP_BYTE_RANGE_TO_EOF)) ? (USHORT)StatusCode::success_ok : (USHORT)StatusCode::success_partial_content;
			response->http_response.pEntityChunks[response->http_response.EntityChunkCount].DataChunkType = HttpDataChunkFromFileHandle;
			response->http_response.pEntityChunks[response->http_response.EntityChunkCount].FromFileHandle.FileHandle = FileH;
			response->http_response.pEntityChunks[response->http_response.EntityChunkCount].FromFileHandle.ByteRange.StartingOffset.QuadPart = start;
			if (end == HTTP_BYTE_RANGE_TO_EOF)
			{
				response->http_response.pEntityChunks[response->http_response.EntityChunkCount].FromFileHandle.ByteRange.Length.QuadPart = end;
			}
			else
				response->http_response.pEntityChunks[response->http_response.EntityChunkCount].FromFileHandle.ByteRange.Length.QuadPart = end - start + 1;

			response->http_response.Version = HTTP_VERSION_1_1;


			response->http_response.EntityChunkCount++;

			if (response->http_response.StatusCode == (USHORT)StatusCode::success_partial_content)
			{
				contentRange = fmt::format("bytes {}-{}/{}", start, end, ul.QuadPart);
			}
			break; // TODO -> Only the furst one is supported for now

		}
		return true;
	}

	void HTTPServer2::ProcessReceiveAndPostResponse(
		HTTP_IOREQUEST* pIoRequest,
		PTP_IO Io,
		ULONG IoResult
	)
	{
		std::wstring w = L"HTTP THREAD " + std::to_wstring(GetCurrentThreadId());


#ifdef NAME_THE_THREAD
		HRESULT r;
		r = SetThreadDescription(
			GetCurrentThread(),
			w.c_str()
		);
#endif

		ULONG Result;
		std::string cacheData{};
		//HTTP_CACHE_POLICY CachePolicy;
		HTTP_IORESPONSE* pIoResponse;
		SERVER_CONTEXT* pServerContext;

		pServerContext = pIoRequest->__io_context__.pServerContext;




		if (((pIoRequest->__p_http_request__->Flags & HTTP_REQUEST_FLAG_MORE_ENTITY_BODY_EXISTS) == HTTP_REQUEST_FLAG_MORE_ENTITY_BODY_EXISTS)
			&& (IoResult == NO_ERROR))
		{

			std::string sz(pIoRequest->__p_http_request__->Headers.KnownHeaders[HttpHeaderContentLength].pRawValue, pIoRequest->__p_http_request__->Headers.KnownHeaders[HttpHeaderContentLength].RawValueLength);
			if (!sz.empty())
			{
#ifdef _WIN64
				size_t size = atoll(sz.c_str());
#else
				size_t size = atol(sz.c_str());
#endif
				unsigned long bytesReturned = 0;
				pIoRequest->__body_buffer__.resize(size);
				Result = HttpReceiveRequestEntityBody(
					pServerContext->hRequestQueue,
					pIoRequest->__p_http_request__->RequestId,
					HTTP_RECEIVE_REQUEST_ENTITY_BODY_FLAG_FILL_BUFFER,
					pIoRequest->__body_buffer__.data(),
					(unsigned long)pIoRequest->__body_buffer__.size(),
					&bytesReturned,
					nullptr
				);

				if ((bytesReturned != pIoRequest->__body_buffer__.size()) || (Result != NOERROR))
				{
					IoResult = ERROR_MORE_DATA; //fake it
				}
				else
				{
					IoResult = NOERROR;
					pIoRequest->__p_http_request__->EntityChunkCount = 1;
					pIoRequest->__p_http_request__->pEntityChunks = &pIoRequest->__chunk__;
					pIoRequest->__chunk__.DataChunkType = HttpDataChunkFromMemory;
					pIoRequest->__chunk__.FromMemory.pBuffer = pIoRequest->__body_buffer__.data();
					pIoRequest->__chunk__.FromMemory.BufferLength = (unsigned long)pIoRequest->__body_buffer__.size();


				}
			}
		}

		StatusCode res = StatusCode::server_error_internal_server_error;

		pIoResponse = AllocateHttpIoResponse(pServerContext);



		switch (IoResult) {
		case NO_ERROR:
		{
			if (pIoResponse == nullptr) break;

			//mulmap_of_str_ci headers; // TODO FIXME!!!!!
			//pIoRequest->get_headers(headers);



			//auto upd = headers.find("Upgrade");

			//if (upd != headers.end())
			//{
			//	std::string upgradeHeader = toLower(upd->second);;
			//	std::string connectionHeader;

			//	auto conn = headers.find("Connection");
			//	if (conn != headers.end())
			//	{
			//		connectionHeader = toLower(conn->second);
			//	}


			//	if ((connectionHeader == "upgrade") && (upgradeHeader == "websocket"))
			//	{
			//		//We might want to upgrade to websocket ... or not
			//		pIoResponse->reply.message = "";
			//		pIoResponse->reply.mimeType = "";
			//		pIoResponse->reply.reason = "";
			//		//pIoResponse->reply.headers.insert()
			//		res = StatusCode::client_error_bad_request;
			//		break;
			//		//		bAuthenticationRequired = true;
			//	}
			//}



			try
			{
#ifdef HTTP_LOGING
				LOG::info("HTTP SERVER RECEIVE", "URL: {}, BODY: {}", pIoRequest->get_relative_url(), pIoRequest->get_body_as_string());
#endif
				pIoRequest->setParamsAndHeaders();

				pIoResponse->reply = {};



				res = pServerContext->svrClass->Router(*pIoRequest, pIoResponse->reply);

#ifdef HTTP_LOGING
				LOG::info("HTTP SERVER RESPONSE", "Message: {}, Reason: {}, StatusCode: {}", pIoResponse->reply.message, pIoResponse->reply.reason, (uint32_t)res);
#endif
			}
			catch (const std::exception& e)
			{
				std::string message = "Exception in ";
				message.append(__FUNCTION__);
				message.append(" ::: ");
				message.append(e.what());

				res = pIoResponse->reply.DoError(StatusCode::server_error_internal_server_error, ErrorResponse<StatusCode>{ StatusCode::server_error_internal_server_error, message });
			}


			break;
		}
		case ERROR_MORE_DATA:
		{
			res = StatusCode::client_error_payload_too_large;
			pIoResponse->reply.message = "";
			pIoResponse->reply.reason = "";

			break;
		}
		default:
			// If the HttpReceiveHttpRequest call failed asynchronously
			// with a different error than ERROR_MORE_DATA, the error is fatal
			// There's nothing this function can do

			CleanupHttpIoResponse(pIoResponse);
			return;
		}




		if (pIoResponse == NULL)
		{
			return;
		}

		if (pIoResponse->reply.message.empty() && (res != StatusCode::success_ok))
		{
			res = pIoResponse->reply.DoError<StatusCode>(res, { res, fmt::format("{} {}", res, status_code(res)) });
		}


		pIoResponse->http_response.StatusCode = (USHORT)res;

		pIoResponse->http_response.EntityChunkCount = 0;
		std::string content_range{};
		if (pIoResponse->reply.type != rt_FILE)
		{
			if (pIoResponse->reply.message.empty())
			{
				pIoResponse->reply.type = rt_TEXT;
				pIoResponse->http_response.EntityChunkCount = 0;
			}
			else
			{
				pIoResponse->http_response.EntityChunkCount = 1;
				pIoResponse->http_response.pEntityChunks[0].DataChunkType = HttpDataChunkFromMemory;
				pIoResponse->http_response.pEntityChunks[0].FromMemory.BufferLength = (unsigned long)pIoResponse->reply.message.length();
				pIoResponse->http_response.pEntityChunks[0].FromMemory.pBuffer = (PVOID)pIoResponse->reply.message.data();
			}


			if (pIoResponse->reply.reason.empty())
			{
				pIoResponse->reply.reason = status_code(res);
			}

		}
		else
		{
			if (res == StatusCode::success_ok)
			{
				auto rangeIt = pIoRequest->headers.find("range");
				std::vector < std::pair<uint64_t, uint64_t>> ranges;
				if (rangeIt != pIoRequest->headers.end())
				{
					std::string rangeV = rangeIt->second;
					getRanges(rangeV, ranges);
				}
				if (ranges.empty())
				{
					ranges.push_back(std::make_pair(0, HTTP_BYTE_RANGE_TO_EOF));
				}

				pIoResponse->reply.reason.clear();
				auto response = PrepareFileResponse(pIoResponse->reply.message, ranges, pIoResponse, content_range);
			}
		}


		if (pIoResponse->reply.reason.empty())
		{
			pIoResponse->reply.reason = status_code((StatusCode)pIoResponse->http_response.StatusCode);
		}

		pIoResponse->http_response.ReasonLength = (USHORT)pIoResponse->reply.reason.length();
		pIoResponse->http_response.pReason = nullptr;
		std::string mime = get_mime_type_from_reply(pIoResponse->reply);

		if (pIoResponse->http_response.ReasonLength > 0)
		{
			pIoResponse->http_response.pReason = pIoResponse->reply.reason.data();
		}

		pIoResponse->http_response.Headers.KnownHeaders[HttpHeaderContentType].pRawValue = mime.c_str();
		pIoResponse->http_response.Headers.KnownHeaders[HttpHeaderContentType].RawValueLength = (USHORT)mime.length();

		if (pIoResponse->reply.expireCacheSec > 0)
		{
			cacheData = fmt::format("max-age={}", pIoResponse->reply.expireCacheSec);
			pIoResponse->http_response.Headers.KnownHeaders[HttpHeaderCacheControl].pRawValue = cacheData.c_str();
			pIoResponse->http_response.Headers.KnownHeaders[HttpHeaderCacheControl].RawValueLength = (uint16_t)cacheData.size();
		}
		else
		{
			pIoResponse->http_response.Headers.KnownHeaders[HttpHeaderCacheControl].pRawValue = CacheControl_NoCache.c_str();
			pIoResponse->http_response.Headers.KnownHeaders[HttpHeaderCacheControl].RawValueLength = (uint16_t)CacheControl_NoCache.size();
		}



		if (pIoResponse->reply.type == rt_FILE)
		{
			if (!content_range.empty())
			{
				pIoResponse->http_response.Headers.KnownHeaders[HttpHeaderContentRange].pRawValue = content_range.c_str();
				pIoResponse->http_response.Headers.KnownHeaders[HttpHeaderContentRange].RawValueLength = (uint16_t)content_range.size();
			}
		}


		StartThreadpoolIo(Io);
		unsigned long flags = 0;
		if (pIoResponse->reply.type != rt_FILE)
		{
			flags = HTTP_SEND_RESPONSE_FLAG_BUFFER_DATA;
			Result = HttpSendHttpResponse(
				pServerContext->hRequestQueue,
				pIoRequest->__p_http_request__->RequestId,
				flags,
				&pIoResponse->http_response,
				NULL,
				NULL,
				NULL,
				0,
				&pIoResponse->__io_context__.Overlapped,
				NULL
			);
			if (Result != NO_ERROR &&
				Result != ERROR_IO_PENDING)
			{
				CancelThreadpoolIo(Io);
				Log::error(__FUNCTION__, "HttpSendHttpResponse failed, error {:#x}", Result);

				CleanupHttpIoResponse(pIoResponse);
			}
		}
		else
		{


			Result = HttpSendHttpResponse(
				pServerContext->hRequestQueue,
				pIoRequest->__p_http_request__->RequestId,
				flags,
				&pIoResponse->http_response,
				NULL,
				NULL,
				NULL,
				0,
				NULL,//&pIoResponse->__io_context__.Overlapped,
				NULL
			);
			if ((pIoResponse->http_response.EntityChunkCount) && (pIoResponse->http_response.pEntityChunks->DataChunkType == HttpDataChunkFromFileHandle))
			{
				CloseHandle(pIoResponse->http_response.pEntityChunks->FromFileHandle.FileHandle);
			}

			if (Result != NO_ERROR &&
				Result != ERROR_IO_PENDING)
			{
				CancelThreadpoolIo(Io);
				Log::error(__FUNCTION__, "HttpSendHttpResponse failed, error {:#x}", Result);

				CleanupHttpIoResponse(pIoResponse);
			}

		}




	}

	StatusCode HTTPServer2::Router(HTTP_IOREQUEST& request, HTTP_REPLY& reply)
	{
		//cookie
		std::shared_lock<std::shared_mutex> lockit(m_configMutex);
		std::string _rela_url = request.get_relative_url();
		std::string _method = request.method_as_string();



		if ((_rela_url == "/help/api_urls") && (_method == HTTP::methods::s_get_method))
		{
			auto response = nlohmann::json::array();
			for (auto& ep : _registered_endpoints)
			{
				nlohmann::json jsLink;
				jsLink["url"] = ep.relative_url;
				jsLink["method"] = ep.method;
				response.push_back(jsLink);
			}
			reply.message = response.dump();
			reply.type = rt_JSON;
			return StatusCode::success_ok;
		}

		if (_registered_endpoints.empty())
		{
			return reply.DoError<StatusCode>(StatusCode::client_error_not_found, { StatusCode::client_error_not_found, fmt::format("URL:: {} not found on this server", request.get_relative_url()) });
		}

		auto ep = std::find_if(_registered_endpoints.begin(), _registered_endpoints.end(), [&_method, &_rela_url](const ENDPOINT& ep) {
			return (ep.method == _method) && (ep.relative_url == _rela_url);
			});
		if (ep != _registered_endpoints.end())
		{
			//we are checking for nullptr on add
			return ep->callbackFn(cookie, request, reply);
		}

		//Part two
		mulmap_of_str_ci localParams;

		for (auto ep = _registered_endpoints.begin(); ep != _registered_endpoints.end(); ep++)
		{
			if (_method != ep->method) continue;

			auto& uridef = ep->relative_url;
			localParams.clear();
			//request /a/b/c/d
			//endpoint /a/{testid}/{catchme}/d


			auto startIter = uridef.begin();
			auto reqStartIter = _rela_url.begin();
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

					auto reqposIt = std::search(reqStartIter, _rela_url.end(), std::default_searcher(current.begin(), current.end()));



					if (reqposIt == _rela_url.end())
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
					if (reqStartIter >= _rela_url.end()) {
						localParams.clear();
						break;
					}

					auto dstPos = std::find(reqStartIter, _rela_url.end(), '/');



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


			std::string req{};
			std::string mine{};

			if (reqStartIter != _rela_url.end())
			{
				req = std::string(reqStartIter, _rela_url.end());
			}

			if (startIter != uridef.end())
			{
				mine = std::string(startIter, uridef.end());
			}

			std::string  tmp_uridef = uridef;
			bool test_ok = false;
			if (tmp_uridef.back() == '*')
			{
				tmp_uridef.resize(tmp_uridef.size() - 1);
				std::string rela_tmp = _rela_url.substr(0, tmp_uridef.size());
				test_ok = (rela_tmp == tmp_uridef);
			}
			if ((localParams.size() > 0) || (test_ok))
			{


				if (!req.empty() && !mine.empty() && (req == mine))
				{
					request.params.insert(localParams.begin(), localParams.end());
					return ep->callbackFn(cookie, request, reply);
				}

				if (!req.empty() && !mine.empty() && (mine == "/*"))
				{
					request.params.insert(localParams.begin(), localParams.end());
					return ep->callbackFn(cookie, request, reply);
				}

				if (req.empty() && mine.empty())
				{
					request.params.insert(localParams.begin(), localParams.end());
					return ep->callbackFn(cookie, request, reply);
				}

			}
			else
			{
				localParams.clear();
			}

		}

		//LastChance
		if ((request.method_as_string() == "GET") && (!_fullFsPath.empty()) && (_rela_url.find("..") == _rela_url.npos))
		{
			using namespace std::filesystem;

			std::string file_path;
			if (_rela_url == "/")
			{
				file_path = _fullFsPath + "/" + _indexFile;
			}
			else
			{
				file_path = _fullFsPath + _rela_url;
			}


			std::replace(file_path.begin(), file_path.end(), '/', '\\');
			//path p(utf8_to_wstring( file_path));

			file_status stat = std::filesystem::status(utf8_to_wstring(file_path));
			if (stat.type() == file_type::regular)
			{
				reply.type = rt_FILE;
				reply.message = file_path;
				return StatusCode::success_ok;
			}


		}


		//We didn't find anything
		return reply.DoError<StatusCode>(StatusCode::client_error_not_found, { StatusCode::client_error_not_found, fmt::format("URL:: {} not found on this server", request.get_relative_url()) });
	}






	bool HTTPServer2::StartServer(
		SERVER_CONTEXT& ServerContext
	)
	{
		DWORD_PTR dwProcessAffinityMask, dwSystemAffinityMask;
		WORD wRequestsCounter;
		BOOL bGetProcessAffinityMaskSucceed;

		bGetProcessAffinityMaskSucceed = GetProcessAffinityMask(
			GetCurrentProcess(),
			&dwProcessAffinityMask,
			&dwSystemAffinityMask);

		if (bGetProcessAffinityMaskSucceed)
		{
			for (wRequestsCounter = 0; dwProcessAffinityMask; dwProcessAffinityMask >>= 1)
			{
				if (dwProcessAffinityMask & 0x1) wRequestsCounter++;
			}

			wRequestsCounter = REQUESTS_PER_PROCESSOR * wRequestsCounter;
		}
		else
		{
			Log::error(__FUNCTION__, "We could not calculate the number of processor's, "
				"the server will continue with the default number = {}", OUTSTANDING_REQUESTS);


			wRequestsCounter = OUTSTANDING_REQUESTS;
		}

		for (; wRequestsCounter > 0; --wRequestsCounter)
		{
			HTTP_IOREQUEST* pIoRequest = nullptr;
			ULONG Result;

			pIoRequest = AllocateHttpIoRequest(ServerContext);

			if (pIoRequest == NULL)
			{
				Log::error(__FUNCTION__, "AllocateHttpIoRequest failed for context {}", wRequestsCounter);
				return false;
			}

			StartThreadpoolIo(ServerContext.Io);

			Result = HttpReceiveHttpRequest(
				ServerContext.hRequestQueue,
				HTTP_NULL_ID,
				0,//Headers only HTTP_RECEIVE_REQUEST_FLAG_COPY_BODY,
				pIoRequest->__p_http_request__,
				(uint32_t)pIoRequest->__http_request_buffer__.size(),
				NULL,
				&pIoRequest->__io_context__.Overlapped
			);

			if (Result != ERROR_IO_PENDING && Result != NO_ERROR)
			{
				CancelThreadpoolIo(ServerContext.Io);

				if (Result == ERROR_MORE_DATA)
				{
					ProcessReceiveAndPostResponse(pIoRequest, ServerContext.Io, ERROR_MORE_DATA);
				}

				CleanupHttpIoRequest(pIoRequest);


				Log::error(__FUNCTION__, "HttpReceiveHttpRequest failed, error {:#x}", Result);

				return false;
			}
		}

		return true;
	}


	static void CALLBACK IoCompletionCallback(
		PTP_CALLBACK_INSTANCE Instance,
		PVOID pContext,
		PVOID pOverlapped,
		ULONG IoResult,
		ULONG_PTR NumberOfBytesTransferred,
		PTP_IO Io
	)
	{
		SERVER_CONTEXT* pServerContext;

		UNREFERENCED_PARAMETER(NumberOfBytesTransferred);
		UNREFERENCED_PARAMETER(Instance);
		UNREFERENCED_PARAMETER(pContext);

		HTTP_IO_CONTEXT* pIoContext = CONTAINING_RECORD(pOverlapped,
			HTTP_IO_CONTEXT,
			Overlapped);

		pServerContext = pIoContext->pServerContext;

		pIoContext->pfCompletionFunction(pIoContext, Io, IoResult);
	}

	bool HTTPServer2::InitializeServerIo(
		SERVER_CONTEXT& ServerContext
	)
	{
		ULONG Result;
		HTTP_BINDING_INFO HttpBindingInfo = { 0 };

		Result = HttpCreateRequestQueue(
			HttpApiVersion,
			nullptr,
			NULL,
			0,
			&(ServerContext.hRequestQueue));

		if (Result != NO_ERROR)
		{
			Log::error(__FUNCTION__, "HttpCreateRequestQueue failed");
			return FALSE;
		}

		HttpBindingInfo.Flags.Present = 1;
		HttpBindingInfo.RequestQueueHandle = ServerContext.hRequestQueue;

		Result = HttpSetUrlGroupProperty(
			ServerContext.urlGroupId,
			HttpServerBindingProperty,
			&HttpBindingInfo,
			sizeof(HttpBindingInfo));

		if (Result != NO_ERROR)
		{
			Log::error(__FUNCTION__, "HttpSetUrlGroupProperty(...HttpServerBindingProperty...) failed");
			return FALSE;
		}

		ServerContext.Io = CreateThreadpoolIo(
			ServerContext.hRequestQueue,
			IoCompletionCallback,
			NULL,
			NULL);

		if (ServerContext.Io == NULL)
		{
			Log::error(__FUNCTION__, "Creating a new I/O completion object failed");
			return FALSE;
		}

		return TRUE;
	}





	bool HTTPServer2::RemoveRequestURLs()
	{
		if (!m_Initialized.load()) return false;
		std::unique_lock<std::shared_mutex> lockit(m_configMutex);
		ULONG res = HttpRemoveUrlFromUrlGroup(server_context.urlGroupId, nullptr, HTTP_URL_FLAG_REMOVE_ALL);

		if (res != NOERROR)
		{
			Log::error("", "Error {:#x} received in RemoveRequestURLs", res);
			return false;
		}
		return true;
	}

	bool HTTPServer2::add_endpoint(const std::string& method, const std::string& URL, HTTP_CALLBACK_FN Callback)
	{
		if (Callback == nullptr)
		{
#ifdef _DEBUG
			// Callback MUSt not be nullptr
			__debugbreak();
#endif
			return false;
		}
		if (!m_Initialized.load()) return false;
		if (URL.empty()) return false;
		if (*(URL.begin()) != '/')
		{
#ifdef _DEBUG
			//URL MUST start with /
			__debugbreak();
#endif
			return false;
		}
		std::unique_lock<std::shared_mutex> lockit(m_configMutex);
		ENDPOINT ep;
		ep.method = method;
		ep.relative_url = URL;
		ep.callbackFn = Callback;
		_registered_endpoints.push_back(ep);


		return true;
	}

	void HTTPServer2::remove_endpoint(const std::string& method, const std::string& URL)
	{
		if (!m_Initialized.load()) return;
		std::unique_lock<std::shared_mutex> lockit(m_configMutex);
		auto ep = std::remove_if(_registered_endpoints.begin(), _registered_endpoints.end(), [&method, &URL](const ENDPOINT& ep) {
			return (ep.method == method) && (ep.relative_url == URL);
			});

		if (ep != _registered_endpoints.end())
		{
			_registered_endpoints.erase(ep);
		}
	}

	void HTTPServer2::set_document_root(const std::string& fullFsPath, const std::string& indexFileName)
	{
		_fullFsPath = fullFsPath;
		_indexFile = indexFileName;
	}

	bool HTTPServer2::Start()
	{
		if (!m_Initialized.load()) return false;
		std::unique_lock<std::shared_mutex> lockit(m_configMutex);

		if (m_Started) return true;

		{

			if (!StartServer(server_context))
			{
				StopServer(server_context);
				return false;
			}
		}
		m_Started.store(true);
		return m_Started;
	}

}
