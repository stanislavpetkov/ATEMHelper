#pragma once

#ifdef _WINDOWS_
   #pragma message("[" __FILE__ "]:Windows.h is defined before this point!!!! there will be a problem with winsock2.h" ) 
#endif
#include <WinSock2.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <map>
#include "method_types.h"
#include "../API/APICommon.h"


namespace HTTP
{
	enum class Method
	{
		unknown = 0,
		get,
		post,
		put,
		patch,
		del,
		copy,
		head,
		options,
		link,
		unlink,
		purge,
		lock,
		unlock,
		propfind,
		view,
		default = 10000
	};






	struct CRequest
	{
		std::string url;
		std::string method;
		std::vector<std::pair<std::string, std::string>> headers;
		std::string body;
	};


	struct CReply
	{
		std::string mimeType;
		std::string replyText;
		std::map<std::string, std::string> headers;
	};


	typedef StatusCode(*HTTPCallbackFunction)(
		uint64_t HTTPServerCookie,
		uint64_t  CallCookie,
		const CRequest & request,
		CReply & reply);

	struct APICALL
	{
		std::string APIKey;
		HTTPCallbackFunction func;
		std::string method;
		uint64_t userData;
	};


	typedef std::vector<APICALL> APICALLS;

	class HTTPServerClass
	{
		friend static void CALLBACK ConnectionTask(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work);
		friend void AcceptServerThread(HTTPServerClass * obj);
	private:
		std::thread m_AcceptThread;

		std::atomic_bool  m_CanNotStart;

		APICALLS m_callbacks;
		uint64_t m_GlobalCookie;

		std::string m_BindIP;
		uint16_t m_Port;
		uint16_t m_ActualPort;

		std::atomic_bool m_Started;
		SOCKET m_ServerSocket;
		HTTPCallbackFunction m_DefaultCallback;

		int BuildServer();
		int TearDown();


		static StatusCode InternalDefaultCallback(
			uint64_t HTTPServerCookie,
			uint64_t  CallCookie,
			const CRequest & request,
			CReply & reply);

		void initFunction(uint64_t GlobalCookie, HTTPCallbackFunction defaultCallback);

	private:
		//For accept thread;
		SOCKET Accept(std::string & remoteAddr, uint16_t & remotePort, sockaddr_in & from);
		StatusCode doCallBack(const CRequest & request, CReply & reply);

		HANDLE closeEvent;
		std::atomic<uint32_t> m_Count;
	public:
		HTTPServerClass();
		HTTPServerClass(uint64_t GlobalCookie);
		HTTPServerClass(uint64_t GlobalCookie, HTTPCallbackFunction defaultCallback);
		~HTTPServerClass();

		int SetEndPoint(const std::string BindIP, uint16_t Port);
		int SetCallBacks(const APICALLS calls);
		int AddCallback(const APICALL call);
		int Start();
		int Stop();


	};

}