#pragma once

#pragma comment(lib, "ws2_32.lib")

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iostream>
#include <vector>
#include <string>
#include <mutex>

class CTCPServer
{
public:
	CTCPServer() { StartUp(); }
	~CTCPServer() { CleanUp(); }

public:
	bool Send(SOCKET Target, const char* Buffer, int BufferLength = -1)
	{
		if (BufferLength == -1) BufferLength = (int)(strlen(Buffer) + 1);
		if (BufferLength <= 0) return false;
		if (BufferLength >= KSendingBufferMaxSize) return false;
		BufferLength += 4; // Four bytes header that contains total byte count
		
		memcpy(m_SendingBuffer, &BufferLength, sizeof(int));
		memcpy(m_SendingBuffer + sizeof(int), Buffer, BufferLength - sizeof(int));

		int ToGo{ BufferLength };
		while (ToGo > 0)
		{
			int SentByteCount{ send(Target, m_SendingBuffer + (BufferLength - ToGo), ToGo, 0) };
			if (SentByteCount < 0)
			{
				std::cerr << "Failed - send(): " << WSAGetLastError() << std::endl;
				return false;
			}
			ToGo -= SentByteCount;
		}
		return true;
	}

	bool SendToAll(const char* Buffer, int BufferLength = -1)
	{
		bool Result{ true };
		for (int i = 1; i < (int)m_SetToBeSelected.fd_count; ++i)
		{
			if (!Send(m_SetToBeSelected.fd_array[i], Buffer, BufferLength))
			{
				Result = false;
			}
		}
		return Result;
	}

	bool SendExcept(SOCKET Exception, const char* Buffer, int BufferLength = -1)
	{
		bool Result{ true };
		for (int i = 1; i < (int)m_SetToBeSelected.fd_count; ++i)
		{
			if (m_SetToBeSelected.fd_array[i] == Exception) continue;
			if (!Send(m_SetToBeSelected.fd_array[i], Buffer, BufferLength))
			{
				Result = false;
			}
		}
		return Result;
	}

	void ReceiveAll()
	{
		fd_set SelectedSet{ m_SetToBeSelected };
		timeval TimeOut{ 2, 0 };
		int SocketCount{ select(0, &SelectedSet, nullptr, nullptr, &TimeOut) };
		for (int i = 0; i < SocketCount; ++i)
		{
			auto& Socket{ SelectedSet.fd_array[i] };
			if (Socket == m_ListeningSocket)
			{
				if (GetClientCount() >= KClientMaxCount)
				{
					std::cerr << "Failed - cannot accept more than " << KClientMaxCount << " clients!" << std::endl;
					continue;
				}

				SOCKET Client{ accept(m_ListeningSocket, nullptr, nullptr) };
				if (Client == INVALID_SOCKET)
				{
					std::cerr << "Failed - accept(): " << WSAGetLastError() << std::endl;
				}
				else
				{
					m_mtxSet.lock();
					{
						FD_SET(Client, &m_SetToBeSelected);
					}
					m_mtxSet.unlock();

					Send(Client, "TCPServer: Welcome to the TCPServer.");
				}
			}
			else
			{
				int ToGo{ 512 };
				int TotalRedeivedByteCount{};
				while (ToGo > 0)
				{
					int ReceivedByteCount{ recv(SelectedSet.fd_array[i], m_ReceivingBuffer + TotalRedeivedByteCount, ToGo, 0) };
					if (ReceivedByteCount <= 0)
					{
						m_mtxSet.lock();
						{
							FD_CLR(SelectedSet.fd_array[i], &m_SetToBeSelected);
						}
						m_mtxSet.unlock();
						break;
					}
					if (TotalRedeivedByteCount == 0)
					{
						memcpy(&ToGo, m_ReceivingBuffer, sizeof(int)); // @important: supposing that first four bytes indicate total length to be received!
					}
					ToGo -= ReceivedByteCount;
					TotalRedeivedByteCount += ReceivedByteCount;
				}

				SendExcept(SelectedSet.fd_array[i], m_ReceivingBuffer + sizeof(int), TotalRedeivedByteCount - sizeof(int));
			}
		}
	}

	void Close()
	{
		m_bIsClosing = true;
	}

public:
	int GetClientCount() const { return (int)(m_SetToBeSelected.fd_count - 1); }
	bool IsClosing() const { return m_bIsClosing; }

public:
	void DisplayInfo()
	{
		sockaddr_in Addr{};
		int AddrLength{ sizeof(Addr) };

		getsockname(m_SetToBeSelected.fd_array[0], (sockaddr*)&Addr, &AddrLength);
		auto& IPv4{ Addr.sin_addr.S_un.S_un_b };
		printf("  [LISTENING SOCKET] IP: %d.%d.%d.%d  PORT: %d(H) %d(N)\n",
			IPv4.s_b1, IPv4.s_b2, IPv4.s_b3, IPv4.s_b4, ntohs(Addr.sin_port), Addr.sin_port);

		for (int i = 1; i < (int)m_SetToBeSelected.fd_count; ++i)
		{
			getsockname(m_SetToBeSelected.fd_array[i], (sockaddr*)&Addr, &AddrLength);
			auto& IPv4{ Addr.sin_addr.S_un.S_un_b };
			printf("  [CLIENT SOCKET %d] IP: %d.%d.%d.%d  PORT: %d(H) %d(N)\n",
				m_SetToBeSelected.fd_array[i], IPv4.s_b1, IPv4.s_b2, IPv4.s_b3, IPv4.s_b4, ntohs(Addr.sin_port), Addr.sin_port);
		}
	}

private:
	bool StartUp()
	{
		{
			WSADATA wsaData{};
			int Error{ WSAStartup(MAKEWORD(2, 2), &wsaData) };
			if (Error)
			{
				std::cerr << "Failed - WSAStartup(): " << Error << std::endl;
				return false;
			}
		}

		m_ListeningSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (m_ListeningSocket == INVALID_SOCKET)
		{
			std::cerr << "Failed - socket(): " << WSAGetLastError() << std::endl;
			m_ListeningSocket = 0;
			return false;
		}

		char HostName[256]{};
		if (gethostname(HostName, 255) == SOCKET_ERROR)
		{
			std::cerr << "Failed - gethostname(): " << WSAGetLastError() << std::endl;
			return false;
		}

		ADDRINFOA AddrInfoHint{};
		AddrInfoHint.ai_family = AF_INET;
		AddrInfoHint.ai_socktype = SOCK_STREAM;
		AddrInfoHint.ai_protocol = IPPROTO_TCP;
		ADDRINFOA* AddrInfo{};
		{
			int Error{ getaddrinfo(HostName, nullptr, &AddrInfoHint, &AddrInfo) };
			if (Error)
			{
				std::cerr << "Failed - getaddrinfo(): " << Error << std::endl;
				return false;
			}
		}
		memcpy(&m_ServerAddr, AddrInfo->ai_addr, sizeof(m_ServerAddr));
		freeaddrinfo(AddrInfo);

		m_ServerAddr.sin_port = htons(KServicePort);
		if (bind(m_ListeningSocket, (sockaddr*)&m_ServerAddr, sizeof(m_ServerAddr)) == SOCKET_ERROR)
		{
			std::cerr << "Failed - bind(): " << WSAGetLastError() << std::endl;
			return false;
		}

		if (listen(m_ListeningSocket, SOMAXCONN) == SOCKET_ERROR)
		{
			std::cerr << "Failed - listen(): " << WSAGetLastError() << std::endl;
			return false;
		}

		FD_SET(m_ListeningSocket, &m_SetToBeSelected);

		printf("[SYSTEM] TCPServer is operational\n");

		return true;
	}

	bool CleanUp()
	{
		if (m_ListeningSocket && closesocket(m_ListeningSocket) == SOCKET_ERROR)
		{
			std::cerr << "Failed to close ListeningSocket: " << WSAGetLastError() << std::endl;
		}
		FD_CLR(m_ListeningSocket, &m_SetToBeSelected.fd_array);

		for (int i = 0; i < (int)m_SetToBeSelected.fd_count; ++i)
		{
			if (closesocket(m_SetToBeSelected.fd_array[i]) == SOCKET_ERROR)
			{
				std::cerr << "Failed - closesocket(): " << WSAGetLastError() << std::endl;
			}
		}

		int Error{ WSACleanup() };
		if (Error)
		{
			std::cerr << "Failed - WSACleanup(): " << Error << std::endl;
			return false;
		}
		return true;
	}

private:
	static constexpr u_short KServicePort{ 9999 };
	static constexpr int KBufferSize{ 1024 };
	static constexpr int KSendingBufferMaxSize{ KBufferSize - 4 };
	static constexpr int KClientMaxCount{ 63 };

private:
	SOCKET m_ListeningSocket{};
	SOCKADDR_IN m_ServerAddr{};

private:
	fd_set m_SetToBeSelected{};
	std::mutex m_mtxSet{};

private:
	char m_ReceivingBuffer[KBufferSize]{};
	char m_SendingBuffer[KBufferSize]{};

private:
	bool m_bIsClosing{};
};
