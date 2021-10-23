
#ifdef _WIN32
#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
//#define _CRT_SECURE_NO_WARNINGS // _CRT_SECURE_NO_WARNINGS for sscanf errors in MSVC2013 Express
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <fcntl.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#pragma comment( lib, "ws2_32" )
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <io.h>
#ifndef _SSIZE_T_DEFINED
typedef int ssize_t;
#define _SSIZE_T_DEFINED
#endif
#ifndef _SOCKET_T_DEFINED
typedef SOCKET socket_t;
#define _SOCKET_T_DEFINED
#endif
#ifndef snprintf
#define snprintf _snprintf_s
#endif
#if _MSC_VER >=1600
// vs2010 or later
#include <stdint.h>
#else
typedef __int8 int8_t;
typedef unsigned __int8 uint8_t;
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#endif
#define socketerrno WSAGetLastError()
constexpr int SOCKET_EAGAIN_EINPROGRESS = WSAEINPROGRESS;
constexpr int SOCKET_EWOULDBLOCK = WSAEWOULDBLOCK;
#else
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#ifndef _SOCKET_T_DEFINED
typedef int socket_t;
#define _SOCKET_T_DEFINED
#endif
#ifndef INVALID_SOCKET
#define INVALID_SOCKET (-1)
#endif
#ifndef SOCKET_ERROR
#define SOCKET_ERROR   (-1)
#endif
#define closesocket(s) ::close(s)
#include <errno.h>
#define socketerrno errno
constexpr int SOCKET_EAGAIN_EINPROGRESS = EAGAIN;
constexpr int SOCKET_EWOULDBLOCK = EWOULDBLOCK;
#endif

#include <vector>
#include <string>
#include "../../LibLogging/LibLogging.h"
#include "easywsclient.hpp"

using easywsclient::Callback_Imp;
using easywsclient::BytesCallback_Imp;

namespace { // private module-only namespace

	std::string GetSocketErrorText(int err)
	{
		char* s = nullptr;
		const auto fmtRes = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			nullptr, err,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPSTR)&s, 0, nullptr);
		if (0 == fmtRes)
		{
			return fmt::format("Unknown Socket error: {}", err);
		}
		std::string errRes(s, fmtRes);

		LocalFree(s);
		return errRes;
	}

	socket_t hostname_connect(const std::string& hostname, int port) {
		struct addrinfo hints;
		struct addrinfo* result;
		struct addrinfo* p;
		int ret;
		socket_t sockfd = INVALID_SOCKET;

		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;



		std::string sport = std::to_string(port);

		if ((ret = getaddrinfo(hostname.c_str(), sport.c_str(), &hints, &result)) != 0)
		{
			Log::error(__func__, "getaddrinfo: {}", gai_strerrorA(ret));
			return 1;
		}
		for (p = result; p != nullptr; p = p->ai_next)
		{
			sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
			if (sockfd == INVALID_SOCKET) { continue; }
			if (connect(sockfd, p->ai_addr, static_cast<int>(p->ai_addrlen)) != SOCKET_ERROR) {
				break;
			}
			closesocket(sockfd);
			sockfd = INVALID_SOCKET;
		}
		freeaddrinfo(result);
		return sockfd;
	}


	class _DummyWebSocket : public easywsclient::WebSocket
	{
	public:
		void poll(int timeout) { }
		void send(const std::string& message) { }
		void sendBinary(const std::string& message) { }
		void sendBinary(const std::vector<uint8_t>& message) { }
		void sendPing() { }
		void close() { }
		states getReadyState() const { return states::CLOSED; }
		void _dispatch(Callback_Imp& callable) { }
		void _dispatchBinary(BytesCallback_Imp& callable) { }
	};


	class _RealWebSocket : public easywsclient::WebSocket
	{
	public:
		// http://tools.ietf.org/html/rfc6455#section-5.2  Base Framing Protocol
		//
		//  0                   1                   2                   3
		//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		// +-+-+-+-+-------+-+-------------+-------------------------------+
		// |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
		// |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
		// |N|V|V|V|       |S|             |   (if payload len==126/127)   |
		// | |1|2|3|       |K|             |                               |
		// +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
		// |     Extended payload length continued, if payload len == 127  |
		// + - - - - - - - - - - - - - - - +-------------------------------+
		// |                               |Masking-key, if MASK set to 1  |
		// +-------------------------------+-------------------------------+
		// | Masking-key (continued)       |          Payload Data         |
		// +-------------------------------- - - - - - - - - - - - - - - - +
		// :                     Payload Data continued ...                :
		// + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
		// |                     Payload Data continued ...                |
		// +---------------------------------------------------------------+
		struct wsheader_type {
			unsigned header_size;
			bool fin;
			bool mask;
			enum class opcode_t
			{
				CONTINUATION = 0x0,
				TEXT_FRAME = 0x1,
				BINARY_FRAME = 0x2,
				CLOSE = 8,
				PING = 9,
				PONG = 0xa,
			} opcode;
			int N0;
			uint64_t N;
			uint8_t masking_key[4];
		};

		std::vector<uint8_t> rxbuf;
		std::vector<uint8_t> txbuf;
		std::vector<uint8_t> receivedData;

		socket_t sockfd;
		states readyState;
		bool useMask;
		bool isRxBad;

		_RealWebSocket(socket_t sockfd, bool useMask)
			: sockfd(sockfd)
			, readyState(states::OPEN)
			, useMask(useMask)
			, isRxBad(false) {
		}

		states getReadyState() const {
			return readyState;
		}

		void poll(int timeout) { // timeout in milliseconds
			if (readyState == states::CLOSED) {
				if (timeout > 0) {
					timeval tv = { timeout / 1000, (timeout % 1000) * 1000 };
					select(0, nullptr, nullptr, nullptr, &tv);
				}
				return;
			}
			if (timeout != 0) {
				fd_set rfds;
				fd_set wfds;
				timeval tv = { timeout / 1000, (timeout % 1000) * 1000 };
				FD_ZERO(&rfds);
				FD_ZERO(&wfds);
				FD_SET(sockfd, &rfds);
				if (!txbuf.empty()) { FD_SET(sockfd, &wfds); }
				if (const auto ret = select(static_cast<int>(sockfd + 1), &rfds, &wfds, 0, timeout > 0 ? &tv : 0); ret < 1)
				{
					if (ret == SOCKET_ERROR)
					{
						const auto err = WSAGetLastError();
						Log::error(__func__, "Select error: {}", GetSocketErrorText(err));

						close();
						return;

					}
					//ret == 0 - Timeout
					if (txbuf.empty()) return;
				}
			}
			while (true) {
				// FD_ISSET(0, &rfds) will be true
				size_t N = rxbuf.size();
				ssize_t ret;
				rxbuf.resize(N + 1500);
				ret = recv(sockfd, (char*)&rxbuf[0] + N, 1500, 0);

				if (ret < 0 && (socketerrno == SOCKET_EWOULDBLOCK || socketerrno == SOCKET_EAGAIN_EINPROGRESS)) {
					rxbuf.resize(N);
					break;
				}
				else if (ret <= 0) {
					rxbuf.resize(N);
					closesocket(sockfd);
					readyState = states::CLOSED;
					Log::warn(__func__, "{}", ret < 0 ? "Connection error!" : "Connection closed!");
					break;
				}
				else {
					rxbuf.resize(N + ret);
				}
			}
			while (txbuf.size()) {
				int ret = ::send(sockfd, (char*)&txbuf[0], static_cast<int>(txbuf.size()), 0);
				if (false) {} // ??
				else if (ret < 0 && (socketerrno == SOCKET_EWOULDBLOCK || socketerrno == SOCKET_EAGAIN_EINPROGRESS)) {
					break;
				}
				else if (ret <= 0) {
					closesocket(sockfd);
					readyState = states::CLOSED;
					Log::warn(__func__, "{}", ret < 0 ? "Connection error!" : "Connection closed!");
					break;
				}
				else {
					txbuf.erase(txbuf.begin(), txbuf.begin() + ret);
				}
			}
			if (!txbuf.size() && readyState == states::CLOSING) {
				closesocket(sockfd);
				readyState = states::CLOSED;
			}
		}

		// Callable must have signature: void(const std::string & message).
		// Should work with C functions, C++ functors, and C++11 std::function and
		// lambda:
		//template<class Callable>
		//void dispatch(Callable callable)
		virtual void _dispatch(Callback_Imp& callable) {
			struct CallbackAdapter : public BytesCallback_Imp
				// Adapt void(const std::string<uint8_t>&) to void(const std::string&)
			{
				Callback_Imp& callable;
				CallbackAdapter(Callback_Imp& callable) : callable(callable) { }
				void operator()(const std::vector<uint8_t>& message) {
					std::string stringMessage(message.begin(), message.end());
					callable(stringMessage);
				}
			};
			CallbackAdapter bytesCallback(callable);
			_dispatchBinary(bytesCallback);
		}

		virtual void _dispatchBinary(BytesCallback_Imp& callable) {
			// TODO: consider acquiring a lock on rxbuf...
			if (isRxBad) {
				return;
			}
			while (true) {
				wsheader_type ws;
				if (rxbuf.size() < 2) { return; /* Need at least 2 */ }
				const uint8_t* data = (uint8_t*)&rxbuf[0]; // peek, but don't consume
				ws.fin = (data[0] & 0x80) == 0x80;
				ws.opcode = (wsheader_type::opcode_t)(data[0] & 0x0f);
				ws.mask = (data[1] & 0x80) == 0x80;
				ws.N0 = (data[1] & 0x7f);
				ws.header_size = 2 + (ws.N0 == 126 ? 2 : 0) + (ws.N0 == 127 ? 8 : 0) + (ws.mask ? 4 : 0);
				if (rxbuf.size() < ws.header_size) { return; /* Need: ws.header_size - rxbuf.size() */ }
				int i = 0;
				if (ws.N0 < 126) {
					ws.N = ws.N0;
					i = 2;
				}
				else if (ws.N0 == 126) {
					ws.N = 0;
					ws.N |= ((uint64_t)data[2]) << 8;
					ws.N |= ((uint64_t)data[3]) << 0;
					i = 4;
				}
				else if (ws.N0 == 127) {
					ws.N = 0;
					ws.N |= ((uint64_t)data[2]) << 56;
					ws.N |= ((uint64_t)data[3]) << 48;
					ws.N |= ((uint64_t)data[4]) << 40;
					ws.N |= ((uint64_t)data[5]) << 32;
					ws.N |= ((uint64_t)data[6]) << 24;
					ws.N |= ((uint64_t)data[7]) << 16;
					ws.N |= ((uint64_t)data[8]) << 8;
					ws.N |= ((uint64_t)data[9]) << 0;
					i = 10;
					if (ws.N & 0x8000000000000000ull) {
						// https://tools.ietf.org/html/rfc6455 writes the "the most
						// significant bit MUST be 0."
						//
						// We can't drop the frame, because (1) we don't we don't
						// know how much data to skip over to find the next header,
						// and (2) this would be an impractically long length, even
						// if it were valid. So just close() and return immediately
						// for now.
						isRxBad = true;
						Log::error(__func__, "Frame has invalid frame length. Closing.");
						close();
						return;
					}
				}
				if (ws.mask) {
					ws.masking_key[0] = ((uint8_t)data[i + 0]) << 0;
					ws.masking_key[1] = ((uint8_t)data[i + 1]) << 0;
					ws.masking_key[2] = ((uint8_t)data[i + 2]) << 0;
					ws.masking_key[3] = ((uint8_t)data[i + 3]) << 0;
				}
				else {
					ws.masking_key[0] = 0;
					ws.masking_key[1] = 0;
					ws.masking_key[2] = 0;
					ws.masking_key[3] = 0;
				}

				// Note: The checks above should hopefully ensure this addition
				//       cannot overflow:
				if (rxbuf.size() < ws.header_size + ws.N) { return; /* Need: ws.header_size+ws.N - rxbuf.size() */ }

				// We got a whole message, now do something with it:
				if (false) {}
				else if (
					ws.opcode == wsheader_type::opcode_t::TEXT_FRAME
					|| ws.opcode == wsheader_type::opcode_t::BINARY_FRAME
					|| ws.opcode == wsheader_type::opcode_t::CONTINUATION
					) {
					if (ws.mask) { for (size_t i = 0; i != ws.N; ++i) { rxbuf[i + ws.header_size] ^= ws.masking_key[i & 0x3]; } }
					receivedData.insert(receivedData.end(), rxbuf.begin() + ws.header_size, rxbuf.begin() + ws.header_size + (size_t)ws.N);// just feed
					if (ws.fin) {
						callable((const std::vector<uint8_t>) receivedData);
						receivedData.erase(receivedData.begin(), receivedData.end());
						std::vector<uint8_t>().swap(receivedData);// free memory
					}
				}
				else if (ws.opcode == wsheader_type::opcode_t::PING) {
					if (ws.mask) { for (size_t i = 0; i != ws.N; ++i) { rxbuf[i + ws.header_size] ^= ws.masking_key[i & 0x3]; } }
					std::string data(rxbuf.begin() + ws.header_size, rxbuf.begin() + ws.header_size + (size_t)ws.N);
					sendData(wsheader_type::opcode_t::PONG, static_cast<uint64_t>(data.size()), data.begin(), data.end());
				}
				else if (ws.opcode == wsheader_type::opcode_t::PONG) {

				}
				else if (ws.opcode == wsheader_type::opcode_t::CLOSE) { close(); }
				else {
					Log::error(__func__, "Got unexpected WebSocket message.");
					close();
				}

				rxbuf.erase(rxbuf.begin(), rxbuf.begin() + ws.header_size + (size_t)ws.N);
			}
		}

		void sendPing() {
			std::string empty;
			sendData(wsheader_type::opcode_t::PING, empty.size(), empty.begin(), empty.end());
		}

		void send(const std::string& message) {
			sendData(wsheader_type::opcode_t::TEXT_FRAME, message.size(), message.begin(), message.end());
		}

		void sendBinary(const std::string& message) {
			sendData(wsheader_type::opcode_t::BINARY_FRAME, message.size(), message.begin(), message.end());
		}

		void sendBinary(const std::vector<uint8_t>& message) {
			sendData(wsheader_type::opcode_t::BINARY_FRAME, message.size(), message.begin(), message.end());
		}

		template<class Iterator>
		void sendData(wsheader_type::opcode_t type, uint64_t message_size, Iterator message_begin, Iterator message_end) {
			// TODO:
			// Masking key should (must) be derived from a high quality random
			// number generator, to mitigate attacks on non-WebSocket friendly
			// middleware:
			const uint8_t masking_key[4] = { 0x12, 0x34, 0x56, 0x78 };
			// TODO: consider acquiring a lock on txbuf...
			if (readyState == states::CLOSING || readyState == states::CLOSED) { return; }
			std::vector<uint8_t> header;
			header.assign(static_cast<size_t>(2) + (message_size >= 126 ? 2 : 0) + (message_size >= 65536 ? 6 : 0) + (useMask ? 4 : 0), 0);
			header[0] = 0x80 | static_cast<uint8_t>(type);
			if (false) {}
			else if (message_size < 126) {
				header[1] = (message_size & 0xff) | (useMask ? 0x80 : 0);
				if (useMask) {
					header[2] = masking_key[0];
					header[3] = masking_key[1];
					header[4] = masking_key[2];
					header[5] = masking_key[3];
				}
			}
			else if (message_size < 65536) {
				header[1] = 126 | (useMask ? 0x80 : 0);
				header[2] = (message_size >> 8) & 0xff;
				header[3] = (message_size >> 0) & 0xff;
				if (useMask) {
					header[4] = masking_key[0];
					header[5] = masking_key[1];
					header[6] = masking_key[2];
					header[7] = masking_key[3];
				}
			}
			else { // TODO: run coverage testing here
				header[1] = 127 | (useMask ? 0x80 : 0);
				header[2] = (message_size >> 56) & 0xff;
				header[3] = (message_size >> 48) & 0xff;
				header[4] = (message_size >> 40) & 0xff;
				header[5] = (message_size >> 32) & 0xff;
				header[6] = (message_size >> 24) & 0xff;
				header[7] = (message_size >> 16) & 0xff;
				header[8] = (message_size >> 8) & 0xff;
				header[9] = (message_size >> 0) & 0xff;
				if (useMask) {
					header[10] = masking_key[0];
					header[11] = masking_key[1];
					header[12] = masking_key[2];
					header[13] = masking_key[3];
				}
			}
			// N.B. - txbuf will keep growing until it can be transmitted over the socket:
			txbuf.insert(txbuf.end(), header.begin(), header.end());
			txbuf.insert(txbuf.end(), message_begin, message_end);
			if (useMask) {
				size_t message_offset = static_cast<size_t>(txbuf.size() - message_size);
				for (size_t i = 0; i != static_cast<size_t>(message_size); ++i) {
					txbuf[message_offset + i] ^= masking_key[i & 0x3];
				}
			}
		}

		void close() {
			if (readyState == states::CLOSING || readyState == states::CLOSED) { return; }
			readyState = states::CLOSING;
			uint8_t closeFrame[6] = { 0x88, 0x80, 0x00, 0x00, 0x00, 0x00 }; // last 4 bytes are a masking key
			std::vector<uint8_t> header(closeFrame, closeFrame + 6);
			txbuf.insert(txbuf.end(), header.begin(), header.end());
		}

	};


	easywsclient::WebSocket::pointer from_url(const std::string& url, bool useMask, const std::string& origin) {

		if (url.size() >= 2048) {
			Log::error(__func__, "url size limit exceeded: {}", url);
			return nullptr;
		}
		if (origin.size() >= 200) {
			Log::error(__func__, "origin size limit exceeded: {}", origin);
			return nullptr;
		}

		enum class mode_t
		{
			host = 1,
			port = 2,
			path = 3

		};

		if ((url.length() < 5) ||
			(url.substr(0, 5) != "ws://"))
		{
			Log::error(__func__, "Could not parse WebSocket url: {}", url);
			return nullptr;
		}

		std::string host; host.reserve(512);
		std::string path; path.reserve(512);

		int32_t port = 0;

		size_t offset = 5;
		mode_t mode = mode_t::host;
		while (true)
		{
			if (offset == url.size()) break;

			const char u = url[offset]; offset++;
			if ((u == ':') && (mode == mode_t::host))
			{
				mode = mode_t::port;
				continue;
			}

			if ((u == '/') && (mode < mode_t::path))
			{
				mode = mode_t(size_t(mode) + 1);
				continue;
			}

			switch (mode)
			{
			case mode_t::host: host += u;
				break;
			case mode_t::port:
			{
				if ((u >= 0x30) && (u <= 0x39))
				{
					port = port * 10 + (uint32_t(u) - 0x30);
					continue;
				}
				mode = mode_t::path;
				offset--;
				continue;
			}
			case mode_t::path:
				if (path.empty() && (u == '?')) path += '/';
				path += u;
				break;
			default:
				break;
			}
		}

	//	path.clear(); Message("REMOVE ME");
		if (path.empty()) path = "/";





		Log::debug(__func__, "easywsclient: connecting: host={} port={} path=/{}", host, port, path);

		socket_t sockfd = hostname_connect(host, port);
		if (sockfd == INVALID_SOCKET) {
			Log::error(__func__, "Unable to connect to: {}:{}", host, port);
			return nullptr;
		}
		{
			// XXX: this should be done non-blocking,
			std::string line;

			int status;

			line = fmt::format("GET {} HTTP/1.1\r\n", path);
			::send(sockfd, line.data(), static_cast<int>(line.size()), 0);

			if (port == 80) {
				line = fmt::format("Host: {}\r\n", host);
				::send(sockfd, line.data(), static_cast<int>(line.size()), 0);
			}
			else {
				line = fmt::format("Host: {}:{}\r\n", host, port);
				::send(sockfd, line.data(), static_cast<int>(line.size()), 0);
			}
			line = "Upgrade: websocket\r\n";
			::send(sockfd, line.data(), static_cast<int>(line.size()), 0);

			line = "Connection: Upgrade\r\n";
			::send(sockfd, line.data(), static_cast<int>(line.size()), 0);

			if (!origin.empty()) {
				line = fmt::format("Origin: {}\r\n", origin);
				::send(sockfd, line.data(), static_cast<int>(line.size()), 0);
			}

			line = "Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==\r\n";
			::send(sockfd, line.data(), static_cast<int>(line.size()), 0);

			line = "Sec-WebSocket-Version: 13\r\n";
			::send(sockfd, line.data(), static_cast<int>(line.size()), 0);

			line = "\r\n";
			::send(sockfd, line.data(), static_cast<int>(line.size()), 0);

			line.clear();
			line.reserve(512);

			char OneChar;
			while (true)
			{
				if (recv(sockfd, &OneChar, 1, 0) == 0) return nullptr;

				line += OneChar;
				if (line.size() < 2) continue;
				if (line[line.size() - 2] == '\r' && line[line.size() - 1] == '\n') break;
				if (line.size() > 4096)
				{
					Log::error(__func__, "Got invalid status line connecting to: {}", url);
					return nullptr;
				}
			}


			if ((line.length() < 10) ||
				(line.substr(0, 9) != "HTTP/1.1 "))
			{
				Log::error(__func__, "Got bad status connecting to {}: {}", url, line);
				return nullptr;
			}

			status = 0;
			for (size_t i = 9; i < line.size(); i++)
			{
				const auto u = line[i];
				if ((u >= 0x30) && (u <= 0x39))
				{
					status = status * 10 + (uint32_t(u) - 0x30);
				}
				else break;
			}

			if (status != 101) {
				Log::error(__func__, "Got bad status connecting to {}: {}", url, line);
				return nullptr;
			}
			// TODO: verify response headers,

			while (true)
			{
				line.clear();
				for (size_t i = 0; i < 1023; i++)
				{
					if (recv(sockfd, &OneChar, 1, 0) == 0) return nullptr;
					line += OneChar;
					if (line.size() < 2) continue;
					if (line[line.size() - 2] == '\r' && line[line.size() - 1] == '\n') break;
				}

				if (line[0] == '\r' && line[1] == '\n') break;
			}
		}
		int flag = 1;
		setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag)); // Disable Nagle's algorithm
#ifdef _WIN32
		u_long on = 1;
		ioctlsocket(sockfd, FIONBIO, &on);
#else
		fcntl(sockfd, F_SETFL, O_NONBLOCK);
#endif
		Log::debug(__func__, "connected to: {}", url);
		return easywsclient::WebSocket::pointer(new _RealWebSocket(sockfd, useMask));
	}

} // end of module-only namespace



namespace easywsclient {

	WebSocket::pointer WebSocket::create_dummy() {
		static pointer dummy = pointer(new _DummyWebSocket);
		return dummy;
	}


	WebSocket::pointer WebSocket::from_url(const std::string& url, const std::string& origin) {
		return ::from_url(url, true, origin);
	}

	WebSocket::pointer WebSocket::from_url_no_mask(const std::string& url, const std::string& origin) {
		return ::from_url(url, false, origin);
	}


} // namespace easywsclient
