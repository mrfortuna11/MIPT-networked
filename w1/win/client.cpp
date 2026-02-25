#include <atomic>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <chrono>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "socket_tools.h"


const char* PORT = "2026";

std::atomic<bool> running{true};

std::string buffered_msg;

addrinfo addr_info;
int sfd;

void receive_messages() {
	constexpr size_t buf_size = 1000;
	char buffer[buf_size];
	
	while (running) {
		memset(buffer, 0, buf_size);
		
		sockaddr_in from_addr;
		int from_len = sizeof(sockaddr_in);
		
		int num_bytes = recvfrom(sfd, buffer, buf_size - 1, 0, (sockaddr*)&from_addr, &from_len);
		if (num_bytes > 0) {
			std::cout << "\n" << std::string(buffer, num_bytes) << std::endl;
			std::cout << "> " << std::flush;
		}
		
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

void send_heartbeat() {
	while (running) {
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		if (running) {
			std::string heartbeat = "/ping";
			sendto(sfd, heartbeat.c_str(), heartbeat.size(), 0, addr_info.ai_addr, addr_info.ai_addrlen);
		}
	}
}

void handle_input()
{
	char ch = std::cin.get();
	bool is_printable = ch >= 32 && ch <= 126;
	bool is_backspace = ch == 127 || ch == '\b';
	bool is_enter = ch == '\n';

	if (is_printable)
	{
		buffered_msg += ch;
		std::cout << ch << std::flush;
	}
	else if (is_backspace)
	{
		if (!buffered_msg.empty())
		{
			buffered_msg.pop_back();
			std::cout << "\b \b" << std::flush;
		}
	}
	else if (is_enter)
	{
		if (buffered_msg == "/quit")
		{
			running = false;
			std::cout << "\nExiting...\n";
			std::string quit_msg = "/quit";
			sendto(sfd, quit_msg.c_str(), quit_msg.size(), 0, addr_info.ai_addr, addr_info.ai_addrlen);
		}
		else if (!buffered_msg.empty())
		{
			std::cout << "\nSending: " << buffered_msg << "\n";
			int res =
				sendto(sfd, buffered_msg.c_str(), buffered_msg.size(), 0, addr_info.ai_addr, addr_info.ai_addrlen);
			if (res == SOCKET_ERROR)
			{
				int err = WSAGetLastError();
				std::cout << "Error: " << err << std::endl;
			}

			buffered_msg.clear();
			std::cout << "> " << std::flush;
		}
	}
}

int main(int argc, const char** argv)
{
	std::unique_ptr<WSA> wsa = std::make_unique<WSA>();
	if (!wsa->is_initialized())
	{
		std::cout << "Failed to initialize WSA\n";
		return 1;
	}

	sfd = create_client("localhost", PORT, &addr_info);
	if (sfd == -1)
	{
		std::cout << "Failed to create a socket\n";
		return 1;
	}

	std::cout << "ChatClient - Connected to server on port " << PORT << "\n"
			  << "Commands:\n"
			  << "  /all <message>        - Send to all clients\n"
			  << "  /w <port> <message>   - Send to client with specific port\n"
			  << "  /duel                 - Start a duel\n"
			  << "  /answer <number>      - Submit duel answer\n"
			  << "  /quit                 - Exit\n"
			  << "> ";


	std::thread receiver_thread(receive_messages);
	std::thread heartbeat_thread(send_heartbeat);

	while (running)
	{
		handle_input();
	}

	receiver_thread.join();
	heartbeat_thread.join();

	return 0;
}
