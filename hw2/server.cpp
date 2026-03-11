#include <cstring>
#include <iostream>
#include <memory>
#include <map>
#include <vector>
#include <random>
#include <ws2tcpip.h>
#include <chrono>

#include "socket_tools.h"

const char* PORT = "2026";

struct ClientInfo {
	sockaddr_in address;
	std::chrono::steady_clock::time_point last_seen;
	bool is_alive = true;
};

struct DuelInfo {
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

std::string get_client_key(const sockaddr_in& addr) {
	return std::string(inet_ntoa(addr.sin_addr)) + ":" + std::to_string(ntohs(addr.sin_port));
}

void register_client(const sockaddr_in& addr) {
	std::string key = get_client_key(addr);
	if (clients.find(key) == clients.end()) {
		std::cout << "Client registered: " << key << std::endl;
	}
	clients[key].address = addr;
	clients[key].last_seen = std::chrono::steady_clock::now();
	clients[key].is_alive = true;
}

void cleanup_client(const std::string& client_key) {
	if (clients.find(client_key) != clients.end()) {
		clients.erase(client_key);
		std::cout << "Client disconnected: " << client_key << std::endl;
		
		// Handle duel cleanup if player was in duel
		if (current_duel.active) {
			if (current_duel.player1_key == client_key || current_duel.player2_key == client_key) {
				std::string other_player = (current_duel.player1_key == client_key) ? 
					current_duel.player2_key : current_duel.player1_key;
				std::cout << "Duel cancelled: " << client_key << " disconnected" << std::endl;
				current_duel.active = false;
				current_duel.waiting = false;
			}
		}
	}
}

void cleanup_old_clients() {
		auto now = std::chrono::steady_clock::now();
		std::vector<std::string> to_remove;
		
		for (auto& [key, client] : clients) {
			auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - client.last_seen);
			if (duration.count() > 10) {
				std::cout << "Client timed out: " << key << std::endl;
				to_remove.push_back(key);
			}
		}
		
		for (const auto& key : to_remove) {
			cleanup_client(key);
		}
	}

	void broadcast_message(SOCKET sfd, const std::string& message) {
	for (auto& [key, client] : clients) {
		sendto(sfd, message.c_str(), message.size(), 0, (sockaddr*)&client.address, sizeof(sockaddr_in));
	}
}

int generate_num() {
	static std::random_device rd;
	static std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(1, 100);
	return dis(gen);
}

std::pair<std::string, int> generate_duel() {
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

	fd_set read_set;
	FD_ZERO(&read_set);
	timeval timeout = {0, 100000}; // 100 ms
	auto last_cleanup = std::chrono::steady_clock::now();
	
	while (true)
	{
		FD_SET(sfd, &read_set);
		select((int)sfd + 1, &read_set, NULL, NULL, &timeout);

		// Cleanup disconnected clients periodically
		auto now = std::chrono::steady_clock::now();
		if (std::chrono::duration_cast<std::chrono::seconds>(now - last_cleanup).count() > 5) {
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
				
				if (message == "/ping") {
					continue;
				}
				
				std::cout << "[" << client_key << "]: " << message << std::endl;

				if (message.find("/all ") == 0) {
					std::string broadcast_msg = message.substr(5);
					broadcast_msg = client_key + " says: " + broadcast_msg;
					broadcast_message(sfd, broadcast_msg);
				}
				else if (message.find("/w ") == 0) {
					// Format: "/w <port> <message>"
					size_t space_pos = message.find(' ', 3);
					if (space_pos != std::string::npos) {
						std::string port_str = message.substr(3, space_pos - 3);
						std::string msg = message.substr(space_pos + 1);
						
						// Find client with matching port
						bool found = false;
						for (auto& [key, client] : clients) {
							if (std::to_string(ntohs(client.address.sin_port)) == port_str) {
								std::string private_msg = "[from " + client_key + "]: " + msg;
								sendto(sfd, private_msg.c_str(), private_msg.size(), 0, 
									   (sockaddr*)&client.address, sizeof(sockaddr_in));
								found = true;
								break;
							}
						}
						
						if (!found) {
							std::string error_msg = "Error: Client with port " + port_str + " not found";
							sendto(sfd, error_msg.c_str(), error_msg.size(), 0, 
								   (sockaddr*)&socket_in, socket_len);
						}
					}
				}
				else if (message == "/duel") {
					if (!current_duel.waiting && !current_duel.active) {
						current_duel.waiting = true;
						current_duel.player1 = socket_in;
						current_duel.player1_key = client_key;
						std::string reply = "Waiting for opponent to join duel...";
						sendto(sfd, reply.c_str(), reply.size(), 0, (sockaddr*)&socket_in, socket_len);
					}
					else if (current_duel.waiting && !current_duel.active) {
						// Second player joins - start duel
						current_duel.player2 = socket_in;
						current_duel.player2_key = client_key;
						auto [problem, answer] = generate_duel();
						current_duel.problem = problem;
						current_duel.correct_answer = answer;
						current_duel.active = true;
						current_duel.waiting = false;
						
						std::string duel_msg = "Duel started! Solve: " + problem;
						
						sendto(sfd, duel_msg.c_str(), duel_msg.size(), 0, (sockaddr*)&socket_in, socket_len);
						sendto(sfd, duel_msg.c_str(), duel_msg.size(), 0, 
							   (sockaddr*)&current_duel.player1, sizeof(sockaddr_in));
					}
					else {
						std::string reply = "Duel is already in progress!";
						sendto(sfd, reply.c_str(), reply.size(), 0, (sockaddr*)&socket_in, socket_len);
					}
				}
				else if (message.find("/answer ") == 0) {
					// Format: "/answer <number>"
					if (!current_duel.active) {
						std::string error_msg = "No duel in progress!";
						sendto(sfd, error_msg.c_str(), error_msg.size(), 0, 
							   (sockaddr*)&socket_in, socket_len);
						continue;
					}
					
					// Check if sender is one of the duel participants
					bool is_participant = (client_key == current_duel.player1_key || 
					                        client_key == current_duel.player2_key);
					if (!is_participant) {
						std::string error_msg = "You are not in this duel!";
						sendto(sfd, error_msg.c_str(), error_msg.size(), 0, 
							   (sockaddr*)&socket_in, socket_len);
						continue;
					}
					
					std::string answer_str = message.substr(8);
					try {
						int answer = std::stoi(answer_str);
						
						if (answer == current_duel.correct_answer) {
							std::string winner_msg = client_key + " is the winner!";
							broadcast_message(sfd, winner_msg);
							current_duel.active = false;
							current_duel.waiting = false;
						}
						else {
							std::string wrong_msg = "Wrong answer! Try again.";
							sendto(sfd, wrong_msg.c_str(), wrong_msg.size(), 0, 
								   (sockaddr*)&socket_in, socket_len);
						}
					}
					catch (...) {
						std::string error_msg = "Invalid answer format";
						sendto(sfd, error_msg.c_str(), error_msg.size(), 0, 
							   (sockaddr*)&socket_in, socket_len);
					}
				}
				else if (message == "/quit") {
					cleanup_client(client_key);
				}
			}
		}
	}
	return 0;
}
