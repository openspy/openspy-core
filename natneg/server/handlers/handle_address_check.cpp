#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include "../NNServer.h"
#include "../../tasks/tasks.h"

#include "../NNDriver.h"
#include <OS/Net/IOIfaces/BSDNetIOInterface.h>

#include <jansson.h>

namespace NN {
	void Driver::handle_address_check_packet(OS::Address from, NatNegPacket *packet, std::string gamename) {
		packet->packettype = NN_ADDRESS_REPLY;
		packet->Packet.Init.localip = from.GetInAddr().sin_addr.s_addr;
		packet->Packet.Init.localport = from.GetInAddr().sin_port;
		SendPacket(from, packet);
	}
}