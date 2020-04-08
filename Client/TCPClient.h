#pragma once

#pragma comment(lib, "ws2_32.lib")

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iostream>

class CTCPClient
{
public:
	CTCPClient() { StartUp(); }
	~CTCPClient() { CleanUp(); }

public:
	bool Connect(const char* ServerIP, u_short ServicePort)
	{
		SOCKADDR_IN Addr{};
		Addr.sin_family = AF_INET;
		Addr.sin_port = htons(ServicePort);
		inet_pton(AF_INET, ServerIP, &Addr.sin_addr);
		if (connect(m_ClientSocket, (sockaddr*)&Addr, sizeof(Addr)) == SOCKET_ERROR)
		{
			std::cerr << "Failed - connect(): " << WSAGetLastError() << std::endl;
			return false;
		}
		return true;
	}

	bool Send(const char* Buffer, int BufferLength = -1)
	{
		if (BufferLength == -1) BufferLength = (int)(strlen(Buffer) + 1);
		if (BufferLength <= 0) return false;
		if (BufferLength >= KSendingBufferMaxSize) return false;
		BufferLength += 4; // Four bytes header that contains total byte count
		char* _Buffer = new char[BufferLength] {};
		memcpy(_Buffer, &BufferLength, sizeof(int));
		memcpy(_Buffer + sizeof(int), Buffer, BufferLength - sizeof(int));

		int ToGo{ BufferLength };
		while (ToGo > 0)
		{
			int SentByteCount{ send(m_ClientSocket, _Buffer + (BufferLength - ToGo), ToGo, 0) };
			if (SentByteCount < 0)
			{
				std::cerr << "Failed - send(): " << WSAGetLastError() << std::endl;
				delete[] _Buffer;
				return false;
			}
			ToGo -= SentByteCount;
		}
		delete[] _Buffer;
		return true;
	}

	bool Receive()
	{
		fd_set fdSet{};
		fdSet.fd_count = 1;
		fdSet.fd_array[0] = m_ClientSocket;
		timeval TimeOut{ 2, 0 };
		if (select(0, &fdSet, nullptr, nullptr, &TimeOut) > 0)
		{
			int ToGo{ 512 };
			m_CurrentBufferLength = 0;
			while (ToGo > 0)
			{
				int ReceivedByteCount{ recv(m_ClientSocket, m_Buffer + m_CurrentBufferLength, ToGo, 0) };
				if (ReceivedByteCount < 0)
				{
					std::cerr << "Failed - recv(): " << WSAGetLastError() << std::endl;
					return false;
				}
				if (m_CurrentBufferLength == 0)
				{
					memcpy(&ToGo, m_Buffer, 4); // @important: supposing that first four bytes indicate total length to be received!
				}
				ToGo -= ReceivedByteCount;
				m_CurrentBufferLength += ReceivedByteCount;
			}
			memcpy(m_Buffer, m_Buffer + sizeof(int), m_CurrentBufferLength - sizeof(int)); // remove header
			return true;
		}
		return false;
	}

	void Leave()
	{
		m_bIsLeaving = true;
	}
	
public:
	const char* GetBuffer() const { return m_Buffer; }
	int GetBufferLength() const { return m_CurrentBufferLength; }
	bool IsLeaving() const { return m_bIsLeaving; }

public:
	void DisplayInfo()
	{
		sockaddr_in Addr{};
		int AddrLength{ sizeof(Addr) };
		getsockname(m_ClientSocket, (sockaddr*)&Addr, &AddrLength);

		auto& IPv4{ Addr.sin_addr.S_un.S_un_b };
		printf("  [INFO] IP: %d.%d.%d.%d  PORT: %d(H) %d(N)\n", IPv4.s_b1, IPv4.s_b2, IPv4.s_b3, IPv4.s_b4, ntohs(Addr.sin_port), Addr.sin_port);
	}

private:
	bool StartUp()
	{
		WSADATA wsaData{};
		int Error{ WSAStartup(MAKEWORD(2, 2), &wsaData) };
		if (Error)
		{
			std::cerr << "Failed - WSAStartup(): " << Error << std::endl;
			return false;
		}

		m_ClientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (m_ClientSocket == INVALID_SOCKET)
		{
			std::cerr << "Failed - socket(): " << WSAGetLastError() << std::endl;
			m_ClientSocket = 0;
			return false;
		}

		return true;
	}

	bool CleanUp()
	{
		if (m_ClientSocket && closesocket(m_ClientSocket) == SOCKET_ERROR)
		{
			std::cerr << "Failed - closesocket(): " << WSAGetLastError() << std::endl;
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
	static constexpr int KBufferSize{ 1024 };
	static constexpr int KSendingBufferMaxSize{ KBufferSize - 4 };

private:
	SOCKET m_ClientSocket{};

private:
	char m_Buffer[KBufferSize]{};
	char m_CurrentBufferLength{};

private:
	bool m_bIsLeaving{};
};