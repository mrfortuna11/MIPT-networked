#include <atomic>
#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "socket_tools.h"



const char* PORT = "2026";
const int ACK_TIMEOUT_MS = 1500;
const int MAX_RETRIES = 3;

std::atomic<bool> running{true};

std::string buffered_msg;

addrinfo addr_info;
int sfd;

struct PendingPacket
{
	std::string data;
	uint32_t seq_num;
	std::chrono::steady_clock::time_point sent_time;
	int retry_count = 0;
};

std::map<uint32_t, PendingPacket> pending_packets;
uint32_t packet_seq_counter = 0;
std::chrono::steady_clock::time_point last_heartbeat;

void receive_messages()
{
	constexpr size_t buf_size = 1000;
	char buffer[buf_size];

	while (running)
	{
		memset(buffer, 0, buf_size);

		sockaddr_in from_addr;
		int from_len = sizeof(sockaddr_in);

		int num_bytes = recvfrom(sfd, buffer, buf_size - 1, 0, (sockaddr*)&from_addr, &from_len);
		if (num_bytes > 0)
		{
			std::string message(buffer, num_bytes);

			// Handle ACK messages
			if (message.find("ACK:") == 0)
			{
				try
				{
					uint32_t ack_seq = std::stoul(message.substr(4));
					if (pending_packets.find(ack_seq) != pending_packets.end())
					{
						std::cout << "\n[ACK received for packet " << ack_seq << "]" << std::endl;
						pending_packets.erase(ack_seq);
					}
				}
				catch (...)
				{
					std::cout << "\n[Invalid ACK format]" << std::endl;
				}
			}
			else
			{
				std::cout << "\n" << message << std::endl;
				std::cout << "> " << std::flush;
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

void check_packet_timeouts()
{
	auto now = std::chrono::steady_clock::now();
	std::vector<uint32_t> to_retransmit;

	for (auto it = pending_packets.begin(); it != pending_packets.end(); ++it)
	{
		auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second.sent_time);

		if (elapsed.count() > ACK_TIMEOUT_MS)
		{
			if (it->second.retry_count < MAX_RETRIES)
			{
				to_retransmit.push_back(it->first);
			}
			else
			{
				std::cout << "\n[TIMEOUT] Packet " << it->first << " abandoned after " << MAX_RETRIES << " retries"
						  << std::endl;
				to_retransmit.push_back(it->first); // Remove from pending
			}
		}
	}

	for (uint32_t seq : to_retransmit)
	{
		auto& packet = pending_packets[seq];
		if (packet.retry_count < MAX_RETRIES)
		{
			std::cout << "\n[RETRANSMIT] Packet " << seq << " (attempt " << (packet.retry_count + 1) << ")"
					  << std::endl;
			int res = sendto(sfd, packet.data.c_str(), packet.data.size(), 0, addr_info.ai_addr, addr_info.ai_addrlen);
			if (res != SOCKET_ERROR)
			{
				packet.retry_count++;
				packet.sent_time = std::chrono::steady_clock::now();
			}
		}
		else
		{
			pending_packets.erase(seq);
		}
	}
}

void send_heartbeat()
{
	last_heartbeat = std::chrono::steady_clock::now();

	while (running)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		if (!running)
			break;

		// Check for packet timeouts
		check_packet_timeouts();

		// Send heartbeat
		std::string heartbeat = "/ping";
		int res = sendto(sfd, heartbeat.c_str(), heartbeat.size(), 0, addr_info.ai_addr, addr_info.ai_addrlen);
		if (res == SOCKET_ERROR)
		{
			int err = WSAGetLastError();
			std::cout << "\n[HEARTBEAT ERROR] Error code: " << err << std::endl;
		}

		last_heartbeat = std::chrono::steady_clock::now();
	}
}

void send_message_with_tracking(const std::string& msg)
{
	packet_seq_counter++;

	std::string tracked_msg = "[SEQ:" + std::to_string(packet_seq_counter) + "] " + msg;

	int res = sendto(sfd, tracked_msg.c_str(), tracked_msg.size(), 0, addr_info.ai_addr, addr_info.ai_addrlen);
	if (res == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		std::cout << "\n[SEND ERROR] Error code: " << err << std::endl;
	}
	else
	{
		// Track the packet for retry on timeout
		PendingPacket pkt;
		pkt.data = tracked_msg;
		pkt.seq_num = packet_seq_counter;
		pkt.sent_time = std::chrono::steady_clock::now();
		pkt.retry_count = 0;
		pending_packets[packet_seq_counter] = pkt;
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
			send_message_with_tracking(buffered_msg);
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
