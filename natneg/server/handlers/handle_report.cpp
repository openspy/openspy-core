#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include "../NNServer.h"
#include "../../tasks/tasks.h"

#include "../NNDriver.h"
#include <OS/Net/IOIfaces/BSDNetIOInterface.h>

#include <jansson.h>

namespace NN {
	void Driver::handle_report_packet(OS::Address from, NatNegPacket *packet, std::string gamename) {
		packet->packettype = NN_REPORT_ACK;
		SendPacket(from, packet);
	}
}