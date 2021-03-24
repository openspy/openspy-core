#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include "../NNServer.h"
#include "../../tasks/tasks.h"

#include "../NNDriver.h"
#include <OS/Net/IOIfaces/BSDNetIOInterface.h>

#include <jansson.h>

namespace NN {
    void Driver::handle_preinit_packet(OS::Address from, NatNegPacket *packet, std::string gamename) {
        OS::Address private_address = OS::Address(packet->Packet.Init.localip, ntohs(packet->Packet.Init.localport));
        OS::LogText(OS::ELogLevel_Info, "[%s] Got pre-init - version: %d, client idx: %d, cookie: %d, porttype: %d, use_gameport: %d, private: %s, game: %s", from.ToString().c_str(), packet->version, packet->Packet.Init.clientindex, ntohl(packet->cookie), packet->Packet.Init.porttype, packet->Packet.Init.usegameport, private_address.ToString().c_str(), gamename.c_str());
		packet->packettype = NN_PREINIT_ACK;
        packet->Packet.Preinit.state = NN_PREINIT_READY;
		SendPacket(from, packet);
    }
}