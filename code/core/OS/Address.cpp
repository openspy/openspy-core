#include <OS/OpenSpy.h>
#include <sstream>
namespace OS {

	Address::Address(uint32_t ip, uint16_t port) {
		this->ip = ip;
		this->port = htons(port);
	}
	Address::Address(struct sockaddr_in addr) {
		ip = addr.sin_addr.s_addr;
		port = addr.sin_port;
	}
	Address::Address(std::string input, int input_port) {
		std::string address;
		size_t offset = input.find(':');
		if (offset != std::string::npos) {
			port = htons(atoi(input.substr(offset+1).c_str()));
			address = input.substr(0, offset);
		}
		else {
			address = input;
		}
		ip = inet_addr(address.c_str());

		if(input_port != 0)
			port = htons(input_port);
	}
	Address::Address() {
		ip = 0;
		port = 0;
	}
	uint16_t Address::GetPort() const {
		return ntohs(port);
	}
	const struct sockaddr_in Address::GetInAddr() {
		struct sockaddr_in ret;
		ret.sin_family = AF_INET;
		memset(&ret.sin_zero, 0, sizeof(ret.sin_zero));
		ret.sin_addr.s_addr = ip;
		ret.sin_port = port;
		return ret;

	}
	std::string Address::ToString(bool ip_only) {
		struct sockaddr_in addr;
		addr.sin_port = (port);
		addr.sin_addr.s_addr = (ip);

		char ipinput[64];
		memset(&ipinput, 0, sizeof(ipinput));

		inet_ntop(AF_INET, &(addr.sin_addr), ipinput, sizeof(ipinput));


		std::ostringstream s;
		s << ipinput;
		if (!ip_only) {
			s << ":" << ntohs(port);
		}
		return s.str();
	}

}