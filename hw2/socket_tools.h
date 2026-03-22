#pragma once

#include <cstdint>
#include <string>

struct addrinfo;

// Packet structure for reliable delivery
struct Packet
{
	static constexpr size_t PAYLOAD_SIZE = 900;

	uint32_t sequence_num;
	uint8_t packet_type; // 0=DATA, 1=ACK, 2=HEARTBEAT
	uint32_t payload_size;

	char payload[PAYLOAD_SIZE];

	Packet()
		: sequence_num(0)
		, packet_type(0)
		, payload_size(0)
	{
	}
};

int create_dgram_socket(const char* address, const char* port, addrinfo* res_addr);

int create_server(const char* port);

int create_client(const char* address, const char* port, addrinfo* res_addr);


// Resource Acquisition is Initialization (RAII)
class WSA
{
public:
	WSA();
	~WSA();

	bool is_initialized() { return wsa_initialized; }

private:
	static bool wsa_initialized;
};
