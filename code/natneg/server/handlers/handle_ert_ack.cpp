#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include "../NNServer.h"
#include "../../tasks/tasks.h"

#include "../NNDriver.h"

#include <jansson.h>

namespace NN {
       json_t *get_ert_ack_object(OS::Address from, NatNegPacket *packet, Driver *driver) {
        OS::Address address = driver->GetAddress();
		json_t *packet_obj = json_object();

		json_object_set_new(packet_obj, "address", json_string(from.ToString().c_str()));
        json_object_set_new(packet_obj, "driver_address", json_string(address.ToString().c_str()));
        json_object_set_new(packet_obj, "hostname", json_string(OS::g_hostName));
        int cookie = ntohl(packet->cookie);
        json_object_set_new(packet_obj, "cookie", json_integer(cookie));
        json_object_set_new(packet_obj, "type", json_string("ert_ack"));

        return packet_obj;
   }
    void Driver::handle_ert_ack_packet(OS::Address from, NatNegPacket *packet) {
        json_t *json_obj = get_ert_ack_object(from, packet, this);
        char *json_data = json_dumps(json_obj, 0);

		NNRequestData req;
        req.type = ENNRequestType_SubmitJson;
		req.send_string = json_data;
		AddRequest(req);
        json_decref(json_obj);

		if (json_data)
			free((void *)json_data);


        OS::Address private_address = OS::Address(packet->Packet.Init.localip, ntohs(packet->Packet.Init.localport));
        OS::LogText(OS::ELogLevel_Info, "[%s] Got ERT ack: cookie %d version %d, private: %s", from.ToString(false).c_str(), packet->cookie, packet->version, private_address.ToString(false).c_str());
    }

    
    void Driver::send_ert_test(OS::Address to_address, NatNegPacket *packet) {
        SendPacket(to_address, packet);
        OS::LogText(OS::ELogLevel_Info, "[%s] Send ERT: cookie %d version %d", to_address.ToString(false).c_str(), packet->cookie, packet->version);
    }
}