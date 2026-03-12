#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <set>
#include <vector>
#include <ws2tcpip.h>

#include "socket_tools.h"

const char* PORT = "2026";
const int PACKET_TIMEOUT_MS = 5000;
const int ACK_TIMEOUT_MS = 1000;

std::ofstream error_log("bin/server_errors.log", std::ios::app);

void log_error(const std::string& client_key, const std::string& error_type, const std::string& details)
{
	auto now = std::chrono::system_clock::now();
	auto time = std::chrono::system_clock::to_time_t(now);
	char time_str[100];
	ctime_s(time_str, sizeof(time_str), &time);

	std::string msg = "[" + std::string(time_str).substr(0, 24) + "] CLIENT[" + client_key + "] ERROR: " + error_type +
					  " - " + details;

	error_log << msg << std::endl;
	error_log.flush();

	std::cout << msg << std::endl;
}

struct ClientInfo
{
	sockaddr_in address;
	std::chrono::steady_clock::time_point last_seen;
	bool is_alive = true;
	uint32_t last_seq_received = 0;
	std::set<uint32_t> received_packets;
	std::chrono::steady_clock::time_point last_ack_send;
};

struct DuelInfo
{
	std::string problem;
	int correct_answer;
	sockaddr_in player1;
	sockaddr_in player2;
	std::string player1_key;
	std::string player2_key;
	bool waiting = false;
	bool active = false;
};

std::map<std::string, ClientInfo> clients;
DuelInfo current_duel;

std::string get_client_key(const sockaddr_in& addr)
{
	return std::string(inet_ntoa(addr.sin_addr)) + ":" + std::to_string(ntohs(addr.sin_port));
}

void register_client(const sockaddr_in& addr)
{
	std::string key = get_client_key(addr);
	if (clients.find(key) == clients.end())
	{
		std::cout << "Client registered: " << key << std::endl;
		error_log << "[NEW_CLIENT] " << key << std::endl;
		error_log.flush();
	}
	clients[key].address = addr;
	clients[key].last_seen = std::chrono::steady_clock::now();
	clients[key].last_ack_send = std::chrono::steady_clock::now();
	clients[key].is_alive = true;
}

void cleanup_client(const std::string& client_key)
{
	if (clients.find(client_key) != clients.end())
	{
		clients.erase(client_key);
		std::cout << "Client disconnected: " << client_key << std::endl;
		log_error(client_key, "DISCONNECT", "Client removed from active list");

		// Handle duel cleanup if player was in duel
		if (current_duel.active)
		{
			if (current_duel.player1_key == client_key || current_duel.player2_key == client_key)
			{
				std::string other_player =
					(current_duel.player1_key == client_key) ? current_duel.player2_key : current_duel.player1_key;
				std::cout << "Duel cancelled: " << client_key << " disconnected" << std::endl;
				log_error(client_key, "DUEL_CANCELLED", "Player disconnected from duel");
				current_duel.active = false;
				current_duel.waiting = false;
			}
		}
	}
}

bool is_duplicate_packet(const std::string& client_key, uint32_t seq_num)
{
	if (clients.find(client_key) == clients.end())
	{
		return false;
	}

	if (clients[client_key].received_packets.count(seq_num) > 0)
	{
		return true; // Duplicate detected
	}
	return false;
}

void mark_packet_received(const std::string& client_key, uint32_t seq_num)
{
	if (clients.find(client_key) != clients.end())
	{
		clients[client_key].received_packets.insert(seq_num);
		clients[client_key].last_seq_received = seq_num;

		if (clients[client_key].received_packets.size() > 100)
		{
			auto it = clients[client_key].received_packets.begin();
			clients[client_key].received_packets.erase(it);
		}
	}
}

bool is_out_of_order(const std::string& client_key, uint32_t seq_num)
{
	if (clients.find(client_key) == clients.end())
	{
		return false;
	}

	return seq_num < clients[client_key].last_seq_received;
}

void cleanup_old_clients()
{
	auto now = std::chrono::steady_clock::now();
	std::vector<std::string> to_remove;

	for (auto it = clients.begin(); it != clients.end(); ++it)
	{
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second.last_seen);
		if (duration.count() > PACKET_TIMEOUT_MS)
		{
			std::cout << "Client timed out: " << it->first << std::endl;
			log_error(it->first, "TIMEOUT", "No packets received for " + std::to_string(duration.count()) + "ms");
			to_remove.push_back(it->first);
		}
	}

	for (const auto& key : to_remove)
	{
		cleanup_client(key);
	}
}

void broadcast_message(SOCKET sfd, const std::string& message)
{
	for (auto it = clients.begin(); it != clients.end(); ++it)
	{
		int res = sendto(sfd, message.c_str(), message.size(), 0, (sockaddr*)&it->second.address, sizeof(sockaddr_in));
		if (res == SOCKET_ERROR)
		{
			int err = WSAGetLastError();
			log_error(it->first, "SEND_FAILED", "Broadcast message failed with error code: " + std::to_string(err));
		}
	}
}

void send_ack(SOCKET sfd, const sockaddr_in& addr, uint32_t seq_num)
{
	std::string ack = "ACK:" + std::to_string(seq_num);
	int res = sendto(sfd, ack.c_str(), ack.size(), 0, (sockaddr*)&addr, sizeof(sockaddr_in));
	if (res == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		std::string client_key = get_client_key(addr);
		log_error(client_key, "ACK_FAILED",
			"Failed to send ACK for seq " + std::to_string(seq_num) + ", error: " + std::to_string(err));
	}
}

int generate_num()
{
	static std::random_device rd;
	static std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(1, 100);
	return dis(gen);
}

std::pair<std::string, int> generate_duel()
{
	int a = generate_num();
	int b = generate_num();
	int c = generate_num();

	std::string problem = std::to_string(a) + "*" + std::to_string(b) + "-" + std::to_string(c) + " = ?";
	int answer = a * b - c;
	return {problem, answer};
}

int main(int argc, const char** argv)
{
	std::unique_ptr<WSA> wsa = std::make_unique<WSA>();
	if (!wsa->is_initialized())
	{
		std::cout << "Failed to initialize WSA\n";
		return 1;
	}

	int sfd = create_server(PORT);
	if (sfd == -1)
	{
		std::cout << "Failed to create a socket\n";
		return 1;
	}

	std::cout << "ChatServer - Listening on port: " << PORT << "\n";
	std::cout << "Logging errors to: server_errors.log\n";
	error_log << "=== Server Started ===" << std::endl;
	error_log.flush();

	fd_set read_set;
	FD_ZERO(&read_set);
	timeval timeout = {0, 100000}; // 100 ms
	auto last_cleanup = std::chrono::steady_clock::now();
	uint32_t seq_counter = 0;

	while (true)
	{
		FD_SET(sfd, &read_set);
		select((int)sfd + 1, &read_set, NULL, NULL, &timeout);

		// Cleanup disconnected clients periodically
		auto now = std::chrono::steady_clock::now();
		if (std::chrono::duration_cast<std::chrono::seconds>(now - last_cleanup).count() > 5)
		{
			cleanup_old_clients();
			last_cleanup = now;
		}

		if (FD_ISSET((SOCKET)sfd, &read_set))
		{
			constexpr size_t buf_size = 1000;
			static char buffer[buf_size];
			memset(buffer, 0, buf_size);

			sockaddr_in socket_in;
			int socket_len = sizeof(sockaddr_in);
			int num_bytes = recvfrom((SOCKET)sfd, buffer, buf_size - 1, 0, (sockaddr*)&socket_in, &socket_len);

			if (num_bytes > 0)
			{
				std::string client_key = get_client_key(socket_in);
				register_client(socket_in);

				std::string message(buffer, num_bytes);
				seq_counter++;

				// Check for packet loss (incoming packet detection)
				if (message != "/ping")
				{
					std::cout << "[" << client_key << "] Received packet (seq: " << seq_counter << "): " << message
							  << std::endl;
				}

				// Check for duplicates
				if (is_duplicate_packet(client_key, seq_counter))
				{
					log_error(client_key, "DUPLICATE_PACKET", "Seq#" + std::to_string(seq_counter) + " - discarding");
					continue;
				}

				// Check for out of order
				if (is_out_of_order(client_key, seq_counter))
				{
					log_error(client_key, "OUT_OF_ORDER",
						"Seq#" + std::to_string(seq_counter) + " (last was " +
							std::to_string(clients[client_key].last_seq_received) + ")");
				}

				mark_packet_received(client_key, seq_counter);
				send_ack((SOCKET)sfd, socket_in, seq_counter);

				if (message == "/ping")
				{
					continue;
				}

				if (message.find("/all ") == 0)
				{
					std::string broadcast_msg = message.substr(5);
					broadcast_msg = client_key + " says: " + broadcast_msg;
					broadcast_message(sfd, broadcast_msg);
				}
				else if (message.find("/w ") == 0)
				{
					// Format: "/w <port> <message>"
					size_t space_pos = message.find(' ', 3);
					if (space_pos != std::string::npos)
					{
						std::string port_str = message.substr(3, space_pos - 3);
						std::string msg = message.substr(space_pos + 1);

						// Find client with matching port
						bool found = false;
						for (auto it = clients.begin(); it != clients.end(); ++it)
						{
							if (std::to_string(ntohs(it->second.address.sin_port)) == port_str)
							{
								std::string private_msg = "[from " + client_key + "]: " + msg;
								int res = sendto(sfd, private_msg.c_str(), private_msg.size(), 0,
									(sockaddr*)&it->second.address, sizeof(sockaddr_in));
								if (res == SOCKET_ERROR)
								{
									log_error(it->first, "PRIVATE_MSG_FAILED",
										"Could not send private message from " + client_key);
								}
								found = true;
								break;
							}
						}

						if (!found)
						{
							std::string error_msg = "Error: Client with port " + port_str + " not found";
							log_error(client_key, "CLIENT_NOT_FOUND", "Target port: " + port_str);
							sendto(sfd, error_msg.c_str(), error_msg.size(), 0, (sockaddr*)&socket_in, socket_len);
						}
					}
				}
				else if (message == "/duel")
				{
					if (!current_duel.waiting && !current_duel.active)
					{
						current_duel.waiting = true;
						current_duel.player1 = socket_in;
						current_duel.player1_key = client_key;
						std::string reply = "Waiting for opponent to join duel...";
						sendto(sfd, reply.c_str(), reply.size(), 0, (sockaddr*)&socket_in, socket_len);
					}
					else if (current_duel.waiting && !current_duel.active)
					{
						// Second player joins - start duel
						current_duel.player2 = socket_in;
						current_duel.player2_key = client_key;
						std::pair<std::string, int> duel_pair = generate_duel();
						current_duel.problem = duel_pair.first;
						current_duel.correct_answer = duel_pair.second;
						current_duel.active = true;
						current_duel.waiting = false;

						std::string duel_msg = "Duel started! Solve: " + current_duel.problem;

						sendto(sfd, duel_msg.c_str(), duel_msg.size(), 0, (sockaddr*)&socket_in, socket_len);
						sendto(sfd, duel_msg.c_str(), duel_msg.size(), 0, (sockaddr*)&current_duel.player1,
							sizeof(sockaddr_in));
					}
					else
					{
						std::string reply = "Duel is already in progress!";
						sendto(sfd, reply.c_str(), reply.size(), 0, (sockaddr*)&socket_in, socket_len);
					}
				}
				else if (message.find("/answer ") == 0)
				{
					// Format: "/answer <number>"
					if (!current_duel.active)
					{
						std::string error_msg = "No duel in progress!";
						log_error(client_key, "DUEL_ERROR", "Attempted answer without active duel");
						sendto(sfd, error_msg.c_str(), error_msg.size(), 0, (sockaddr*)&socket_in, socket_len);
						continue;
					}

					// Check if sender is one of the duel participants
					bool is_participant =
						(client_key == current_duel.player1_key || client_key == current_duel.player2_key);
					if (!is_participant)
					{
						std::string error_msg = "You are not in this duel!";
						log_error(client_key, "DUEL_ERROR", "Non-participant attempted to answer");
						sendto(sfd, error_msg.c_str(), error_msg.size(), 0, (sockaddr*)&socket_in, socket_len);
						continue;
					}

					std::string answer_str = message.substr(8);
					try
					{
						int answer = std::stoi(answer_str);

						if (answer == current_duel.correct_answer)
						{
							std::string winner_msg = client_key + " is the winner!";
							broadcast_message(sfd, winner_msg);
							current_duel.active = false;
							current_duel.waiting = false;
						}
						else
						{
							std::string wrong_msg = "Wrong answer! Try again.";
							sendto(sfd, wrong_msg.c_str(), wrong_msg.size(), 0, (sockaddr*)&socket_in, socket_len);
						}
					}
					catch (...)
					{
						std::string error_msg = "Invalid answer format";
						log_error(client_key, "PARSE_ERROR", "Invalid answer format: " + answer_str);
						sendto(sfd, error_msg.c_str(), error_msg.size(), 0, (sockaddr*)&socket_in, socket_len);
					}
				}
				else if (message == "/quit")
				{
					cleanup_client(client_key);
				}
			}
			else if (num_bytes < 0)
			{
				int err = WSAGetLastError();
				if (err != WSAEWOULDBLOCK)
				{
					log_error("SERVER", "RECV_ERROR", "recvfrom failed with error code: " + std::to_string(err));
				}
			}
		}
	}

	error_log << "=== Server Stopped ===" << std::endl;
	error_log.close();
	return 0;
}
