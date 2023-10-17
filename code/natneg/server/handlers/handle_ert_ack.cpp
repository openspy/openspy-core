#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include "../NNServer.h"
#include "../../tasks/tasks.h"

#include "../NNDriver.h"

#include <jansson.h>

namespace NN {
    void Driver::handle_ert_ack_packet(OS::Address from, NatNegPacket *packet) {
        OS::Address private_address = OS::Address(packet->Packet.Init.localip, ntohs(packet->Packet.Init.localport));
        OS::LogText(OS::ELogLevel_Info, "[%s] Got ERT ack: %s", private_address.ToString(false).c_str());
    }
}