#include "HTTPServerClass.h"

#include <Ws2tcpip.h>
#include <algorithm>
#include "../Logging/LocalLog.h"
#include "../StringUtils.h"
//Keep windows last in the chain
#include <Windows.h>

#pragma comment(lib, "Ws2_32.lib")

namespace HTTP
{
	constexpr size_t CNST_SEND_RECEIVE_BUFFER_SIZE = 128 * 1024;


	

	struct CTaskInfo
	{
		HTTPServerClass * obj;
		std::string remoteAddress;
		uint16_t remotePort;
		sockaddr_in remote;

		SOCKET clientSocket;

		HANDLE closeEvent;
	};



	StatusCode ReplyErrorCode(CReply & reply, StatusCode errCode)
	{
		reply.replyText = status_code(errCode);
		reply.mimeType = "text/plain";

		return errCode;
	}


	class CAutoCloseWork
	{
	private:
		PTP_WORK m_thrWork;
	public:
		CAutoCloseWork(PTP_WORK thrWork) :
			m_thrWork(thrWork)
		{
		};

		~CAutoCloseWork()
		{
			CloseThreadpoolWork(m_thrWork);
		};
	};

	std::string StringToUpper(std::string strToConvert)
	{
		std::transform(strToConvert.begin(), strToConvert.end(), strToConvert.begin(), ::toupper);

		return strToConvert;
	}

	std::string trim(std::string& str)
	{
		size_t first = str.find_first_not_of(' ');
		if (first == std::string::npos)
			return "";
		size_t last = str.find_last_not_of(' ');
		return str.substr(first, (last - first + 1));
	}


	bool supportedAccessMethod(const std::string & m)
	{
		for (std::string & var : methods::access_methods)
		{
			if (var == m) return true;
		}
		return false;
	}

	static void CALLBACK ConnectionTask(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work)
	{
		//info(L"NEW TASK - thread %x", GetCurrentThreadId());
		CTaskInfo taskInfo = *(CTaskInfo *)Context;
		delete (CTaskInfo *)Context;
		HTTPServerClass * obj = taskInfo.obj;
		CAutoCloseWork closeWork(Work);
		int bufSize = CNST_SEND_RECEIVE_BUFFER_SIZE;

		std::vector<char> memBuf;
		memBuf.resize(CNST_SEND_RECEIVE_BUFFER_SIZE);


		uint32_t m_BodySize;

		CRequest request = {};
		bool bMethodDone = false;
		bool bURLDone = false;
		bool bKeyMode;
		bool bValueMode;
		bool m_BodyMode = false;
		bool bKeepAlive = false;
		while (WaitForSingleObject(taskInfo.closeEvent, 0) == WAIT_TIMEOUT)
		{
			try
			{
				ZeroMemory(&memBuf.front(), bufSize);
				request.body.clear();
				m_BodySize = 0;
				int rcvSize = recv(taskInfo.clientSocket, &memBuf.front(), bufSize, 0);
				if (rcvSize <= 0)
				{
#ifdef _DEBUG
					SysLoc::debug(__FUNCTION__, "HTTP Server Receive timeout... freeing connection");
#endif
					goto end_task;
				}
				if (rcvSize > 0)
				{

					//those should be headers
					bool bFirstLine = true;
					std::string key, value;
					char lastSymbol = 0;
					for (int i = 0; i < rcvSize; i++)
					{
						if (memBuf[i] == 13) continue;
						if (bFirstLine && (memBuf[i] == 10)) bFirstLine = false;


						if (m_BodyMode)
						{
							if (m_BodySize < 1) break;
							//probably body start

							request.body.push_back(memBuf[i]);
							m_BodySize--;
							continue;
						}


						if ((lastSymbol == 10) && (memBuf[i] == 10))
						{
							m_BodyMode = true;
						}



						lastSymbol = memBuf[i];

						if (bFirstLine)
						{
							if (memBuf[i] != 0x20)
							{


								if (!bMethodDone)
								{
									request.method += memBuf[i];
								}
								else
								{
									if (!bURLDone)
									{
										request.url += memBuf[i];
									}
								}
							}
							else
							{

								if (memBuf[i] == 0x20)
								{
									if (bMethodDone)
									{
										bURLDone = true;
										bKeyMode = true;
										bValueMode = false;
									}
									if (!bMethodDone)
									{
										bMethodDone = true;
									}
								}

							}
						}
						else
						{

							if (!supportedAccessMethod(request.method)) goto end_task; //nothing to be done

							if (memBuf[i] == 10)
							{
								bKeyMode = true;
								bValueMode = false;

								if ((key == "") && (value == "")) continue;

								key = StringToUpper(trim(key));
								value = trim(value);

								if ((key == "") && (value == "")) continue;


								request.headers.push_back(std::pair<std::string, std::string>(key, value));

								if ((key == "CONNECTION") && (toUpperString(value) == "KEEP-ALIVE"))
								{
									bKeepAlive = true;
								}

								if (key == "CONTENT-LENGTH")
								{
									m_BodySize = atoi(value.c_str());
								}
								key = "";
								value = "";
								continue;
							}

							if (bKeyMode && (memBuf[i] == ':'))
							{
								bKeyMode = false;
								bValueMode = true;
								continue;
							}

							if (bKeyMode)
							{
								key += memBuf[i];
							}
							if (bValueMode)
							{
								value += memBuf[i];
							}
						}
					}





 					if (m_BodySize > 0)
					{
						//we need to receive m_BodySize bytes
						while (m_BodySize > 0)
						{
							ZeroMemory(&memBuf.front(), bufSize);
							int rcvSize = recv(taskInfo.clientSocket, &memBuf.front(), min(bufSize, (int)m_BodySize), 0);
							if (rcvSize < 0)
							{
								SysLoc::debug(__FUNCTION__, "Error ::: {}", GetLastError());
								goto end_task;
							}
							if (rcvSize > 0)
							{
								for (int i = 0; i < rcvSize; i++)
								{
									request.body.push_back(memBuf[i]);
									m_BodySize--;
									if (m_BodySize == 0) break;
								}
							}
						}
					}


					CReply reply = {};
					reply.mimeType = "text/html";
					reply.replyText = "";

					StatusCode st = obj->doCallBack(request, reply);


					std::string sndreply = "HTTP/1.1 " + status_code(st) + "\r\n";
					sndreply += "Content-Location: " + request.url + "\r\n";
					sndreply += "Server: PBT API Server\r\n";

					for (std::pair<std::string, std::string> && var : reply.headers)
					{
						std::string key = toUpperString(var.first);
						if (key == "CONTENT-TYPE") continue;
						if (key == "CONTENT-LENGTH") continue;
						if (key == "HOST") continue;
						if (key == "ACCEPT") continue;
						if (key == "ACCEPT-LANGUAGE") continue;
						if (key == "ACCEPT-ENCODING") continue;

						if (key == "USER-AGENT") continue;
						if (key == "UPGRADE-INSECURE-REQUESTS") continue;



						sndreply += var.first + ": " + var.second + "\r\n";
					}

					if ((st != StatusCode::success_ok) && (reply.replyText.empty()))
					{
						ReplyErrorCode(reply, st);
					}


					sndreply += "Content-Type: " + reply.mimeType + "\r\n";
					sndreply += "Content-Length: " + std::to_string(reply.replyText.size());
					sndreply += "\r\n\r\n" + reply.replyText;

					char * ptr = (char *)sndreply.c_str();
					int bytesToSend = (int)sndreply.size();

					while (bytesToSend > 0)
					{
						int res = send(taskInfo.clientSocket, ptr, min(bytesToSend, CNST_SEND_RECEIVE_BUFFER_SIZE), 0);
						if (res <= 0)
						{
							SysLoc::error(__FUNCTION__, "HTTP: Can not send to client...");
							goto end_task; //close connection
						}

						bytesToSend -= res;
						ptr += res;
					}

					//send the data
					request = {};
					m_BodySize = 0;

					bMethodDone = false;
					bURLDone = false;
					bKeyMode = false;
					bValueMode = false;
					m_BodyMode = false;

					if (!bKeepAlive) goto end_task;
				}
			}
			catch (...)
			{
				goto end_task;
			}
		}

	end_task:
		taskInfo.obj->m_Count--;
		shutdown(taskInfo.clientSocket, SD_BOTH);
		closesocket(taskInfo.clientSocket);
	}


	void AcceptServerThread(HTTPServerClass * obj)
	{

		std::string remoteAddress;
		uint16_t remotePort;
		sockaddr_in from;

		while (WaitForSingleObject(obj->closeEvent, 0) == WAIT_TIMEOUT)
		{
			SOCKET clientSocket = obj->Accept(remoteAddress, remotePort, from);

			if (clientSocket == INVALID_SOCKET) continue;

			//do some magic with the thread pool

			CTaskInfo * taskInfo = new CTaskInfo();
			taskInfo->clientSocket = clientSocket;
			taskInfo->closeEvent = obj->closeEvent;
			taskInfo->obj = obj;
			taskInfo->remote = from;
			taskInfo->remoteAddress = remoteAddress;
			taskInfo->remotePort = remotePort;
			obj->m_Count++;
			PTP_WORK thrWork = CreateThreadpoolWork(&ConnectionTask, taskInfo, nullptr);
			SubmitThreadpoolWork(thrWork);
		}
	}

	int HTTPServerClass::BuildServer()
	{
		ResetEvent(closeEvent);

		sockaddr_in server;
		u_long listen_addr = INADDR_ANY;

		inet_pton(AF_INET, m_BindIP.c_str(), &listen_addr);

		server.sin_family = AF_INET;
		server.sin_addr.s_addr = listen_addr;
		server.sin_port = htons((USHORT)m_Port);

		m_ServerSocket = socket(AF_INET, SOCK_STREAM, 0);

		if (m_ServerSocket == INVALID_SOCKET)
		{
			return WSAGetLastError();
		}

		int res = ::bind(m_ServerSocket, (sockaddr*)&server, sizeof(server));

		if (res == SOCKET_ERROR)
		{
			return WSAGetLastError();
		}

		int Flen = sizeof(server);
		if (getsockname(m_ServerSocket, (sockaddr*)&server, &Flen) != -1)
		{
			m_ActualPort = htons(server.sin_port);
		}
		else
		{
			m_ActualPort = 0;
		}

		res = listen(m_ServerSocket, SOMAXCONN);
		if (res != 0)
		{
			return res;
		}


		m_AcceptThread = std::thread(AcceptServerThread, this);

		return 0;
	}

	int HTTPServerClass::TearDown()
	{

		SOCKET tmp = m_ServerSocket;
		m_ServerSocket = INVALID_SOCKET;
		SetEvent(closeEvent);
		shutdown(tmp, SD_BOTH);
		closesocket(tmp);

		if (m_AcceptThread.joinable()) m_AcceptThread.join();
		/*shutdown(tmp, SD_BOTH);
		closesocket(tmp);*/

		return 0;
	}

	StatusCode HTTPServerClass::InternalDefaultCallback(uint64_t HTTPServerCookie,
		uint64_t  CallCookie,
		const CRequest & request,
		CReply & reply)
	{
		reply.replyText = status_code(StatusCode::client_error_not_found);
		reply.mimeType = "text/plain";

		reply.headers.insert(std::make_pair<std::string, std::string>("status", std::to_string((uint32_t)StatusCode::client_error_bad_request)));
		return StatusCode::client_error_not_found;
	}

	void HTTPServerClass::initFunction(uint64_t GlobalCookie, HTTPCallbackFunction defaultCallback)
	{
		m_Count = 0;
		m_GlobalCookie = GlobalCookie;

		if (defaultCallback == nullptr)
		{
			m_DefaultCallback = &HTTPServerClass::InternalDefaultCallback;
		}
		else
		{
			m_DefaultCallback = defaultCallback;
		}



		m_Started = false;
		WSADATA wsaData;
		int iResult = 0;

		iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);

		if (iResult != NO_ERROR) {
			m_CanNotStart = true;
			return;
		}
		closeEvent = CreateEvent(nullptr, true, false, nullptr);
		m_CanNotStart = false;
	}

	HTTPServerClass::HTTPServerClass()
	{
		initFunction(0, nullptr);
	}

	HTTPServerClass::HTTPServerClass(uint64_t GlobalCookie)
	{
		initFunction(GlobalCookie, nullptr);
	}

	HTTPServerClass::HTTPServerClass(uint64_t GlobalCookie, HTTPCallbackFunction defaultCallback)
	{
		initFunction(GlobalCookie, defaultCallback);
	}


	HTTPServerClass::~HTTPServerClass()
	{
		if (m_ServerSocket != INVALID_SOCKET)
		{
			TearDown();
		}
		CloseHandle(closeEvent);
		WSACleanup();
	}

	int HTTPServerClass::SetEndPoint(const std::string BindIP, uint16_t Port)
	{
		m_BindIP = BindIP;
		m_Port = Port;
		return 0;
	}

	int HTTPServerClass::SetCallBacks(const APICALLS calls)
	{
		if (m_Started) return 1;
		m_callbacks = calls;
		return 0;
	}

	int HTTPServerClass::AddCallback(const APICALL call)
	{
		if (m_Started) return 1;
		m_callbacks.push_back(call);
		return 0;
	}

	int HTTPServerClass::Start()
	{
		int res = BuildServer();
		if (res == 0) m_Started = true;
		return res;
	}

	int HTTPServerClass::Stop()
	{
		m_Started = false;

		return TearDown();
	}

	SOCKET HTTPServerClass::Accept(std::string & remoteAddr, uint16_t & remotePort, sockaddr_in & from)
	{
		if (m_CanNotStart) return INVALID_SOCKET;

		int fromlen = sizeof(from);

		SOCKET ClientSocket = ::accept(m_ServerSocket, (sockaddr *)&from, &fromlen);
		if (ClientSocket == INVALID_SOCKET) return INVALID_SOCKET;

		DWORD dwSendTimeout = 1000;
		setsockopt(ClientSocket, SOL_SOCKET, SO_SNDTIMEO, (const char *)&dwSendTimeout, sizeof(dwSendTimeout));
		DWORD dwRcvTimeout = 5000;
		setsockopt(ClientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&dwRcvTimeout, sizeof(dwRcvTimeout));

		DWORD X = CNST_SEND_RECEIVE_BUFFER_SIZE;
		setsockopt(ClientSocket, IPPROTO_TCP, SO_RCVBUF, (const char*)&X, sizeof(DWORD));
		setsockopt(ClientSocket, IPPROTO_TCP, SO_SNDBUF, (const char*)&X, sizeof(DWORD));

		std::string tmp = "000.000.000.000 000.000.000.000 000.000.000.000 000.000.000.000";

		inet_ntop(AF_INET, &from.sin_addr, (PSTR)tmp.c_str(), tmp.size());
		remoteAddr = std::string(tmp.c_str());
		remotePort = ntohs(from.sin_port);

		return ClientSocket;
	}





	StatusCode HTTPServerClass::doCallBack(
		const CRequest & request,
		CReply & reply
	)
	{
		//this might happen on stop
		if (!m_Started) return ReplyErrorCode(reply, StatusCode::server_error_service_unavailable);

		for (APICALL & var : m_callbacks)
		{
			if ((var.method != request.method) && (var.method != "")) continue;
			if (var.APIKey != request.url) continue;

			if (var.func == nullptr) continue;
			try
			{
				return var.func(m_GlobalCookie, var.userData, request, reply);
			}
			catch (...)
			{
				return ReplyErrorCode(reply, StatusCode::server_error_internal_server_error);

			}
		}

		std::string apimodKey = "";
		APICALL api = {};
		bool bFound = false;
		for (APICALL & var : m_callbacks)
		{
			if ((var.method != request.method) && (var.method != "")) continue;
			if (var.APIKey.empty() || (var.APIKey.back() != '*')) continue;

			apimodKey = std::string(var.APIKey.begin(), var.APIKey.end() - 1);
			if (apimodKey.empty()) continue;

			if (request.url.find(apimodKey) == std::string::npos) continue;

			if (var.func == nullptr) continue;
			bFound = true;
			if (api.APIKey.length() < var.APIKey.length())
			{
				api = var;
			}
		}


		if (bFound)
		{
			try
			{
				return api.func(m_GlobalCookie, api.userData, request, reply);
			}
			catch (...)
			{
				return ReplyErrorCode(reply, StatusCode::server_error_internal_server_error);

			}
		}


		return m_DefaultCallback(m_GlobalCookie, 0, request, reply);
	}
}