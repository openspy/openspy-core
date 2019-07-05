#include <server/QRServer.h>
#include <server/QRPeer.h>
#include <tasks/tasks.h>
#include <sstream>
namespace MM {
    bool Handle_ClientMessage(TaskThreadData *thread_data, std::string message) {
		std::string gamename, from_ip, to_ip, from_port, to_port, data, type;
		uint8_t *data_out;
		size_t data_len;

		QR::Server *server = (QR::Server *)thread_data->server;

		std::vector<std::string> vec = OS::KeyStringToVector(message);
		if (vec.size() < 1) return false;
		type = vec.at(0);
		if (!type.compare("send_msg")) {
			if (vec.size() < 7) return false;
			gamename = vec.at(1);
			from_ip = vec.at(2);
			from_port = vec.at(3);
			to_ip = vec.at(4);
			to_port = vec.at(5);
			data = vec.at(6);

			std::ostringstream ss;
			ss << to_ip << ":" << to_port;

			OS::Address address(ss.str().c_str());
			QR::Peer *peer = server->find_client(address);
			if (!peer) {
				return false;							
			}
				
			OS::Base64StrToBin((const char *)data.c_str(), &data_out, data_len);
			peer->SendClientMessage(data_out, data_len);
			free(data_out);
		}
        return true;
    }
}