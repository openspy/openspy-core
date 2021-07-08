#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include "../NNServer.h"
#include "../../tasks/tasks.h"

#include "../NNDriver.h"
#include <OS/Net/IOIfaces/BSDNetIOInterface.h>

#include <jansson.h>

namespace NN {
   json_t *get_natify_object(OS::Address from, NatNegPacket *packet, INetIOSocket *socket) {
		json_t *packet_obj = json_object();
		json_object_set_new(packet_obj, "from_address", json_string(from.ToString().c_str()));
        json_object_set_new(packet_obj, "driver_address", json_string(socket->address.ToString().c_str()));
        json_object_set_new(packet_obj, "hostname", json_string(OS::g_hostName));
        //
        json_object_set_new(packet_obj, "version", json_integer(packet->version));
        json_object_set_new(packet_obj, "type", json_string("natify"));

        json_object_set_new(packet_obj, "cookie", json_integer(ntohl(packet->cookie)));

        json_t *init_obj = json_object();
        json_object_set_new(init_obj, "porttype", json_integer(packet->Packet.Init.porttype));

        OS::Address private_address = OS::Address(packet->Packet.Init.localip, ntohs(packet->Packet.Init.localport));

        json_object_set_new(init_obj, "private_address", json_string(private_address.ToString().c_str()));

        json_object_set_new(packet_obj, "data", init_obj);
        return packet_obj;
   }
    void Driver::handle_natify_packet(OS::Address from, NatNegPacket *packet) {
        OS::LogText(OS::ELogLevel_Info, "[%s] Got natify - version: %d, client idx: %d, porttype: %d", from.ToString().c_str(), packet->version, ntohl(packet->cookie), packet->Packet.Init.porttype);

        if(packet->Packet.Init.porttype == NN_PT_NN1) {
            packet->packettype = NN_ERTTEST;
            SendPacket(from, packet);
            return;
        }

        json_t *json_obj = get_natify_object(from, packet, mp_socket);
        char *json_data = json_dumps(json_obj, 0);

		TaskScheduler<NNRequestData, TaskThreadData> *scheduler = ((NN::Server *)(getServer()))->getScheduler();
		NNRequestData req;
        req.type = ENNRequestType_SubmitJson;
		req.send_string = json_data;
		scheduler->AddRequest(ENNRequestType_SubmitJson, req);
        json_decref(json_obj);

		if (json_data)
			free((void *)json_data);
    }
}