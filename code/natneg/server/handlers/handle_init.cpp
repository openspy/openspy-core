#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include "../NNServer.h"
#include "../../tasks/tasks.h"

#include "../NNDriver.h"
#include <jansson.h>

namespace NN {
   json_t *get_init_object(OS::Address from, NatNegPacket *packet, std::string gamename) {
		json_t *packet_obj = json_object();
		json_object_set_new(packet_obj, "from_address", json_string(from.ToString().c_str()));
        //json_object_set_new(packet_obj, "driver_address", json_string(socket->address.ToString().c_str()));
        json_object_set_new(packet_obj, "hostname", json_string(OS::g_hostName));
        //
        json_object_set_new(packet_obj, "gamename", json_string(gamename.c_str()));
        json_object_set_new(packet_obj, "version", json_integer(packet->version));
        json_object_set_new(packet_obj, "type", json_string("init"));

        json_object_set_new(packet_obj, "cookie", json_integer(ntohl(packet->cookie)));

        json_t *init_obj = json_object();
        json_object_set_new(init_obj, "porttype", json_integer(packet->Packet.Init.porttype));
        json_object_set_new(init_obj, "clientindex", json_integer(packet->Packet.Init.clientindex));
        json_object_set_new(init_obj, "usegameport", json_integer(packet->Packet.Init.usegameport));

        OS::Address private_address = OS::Address(packet->Packet.Init.localip, ntohs(packet->Packet.Init.localport));

        json_object_set_new(init_obj, "private_address", json_string(private_address.ToString().c_str()));

        json_object_set_new(packet_obj, "data", init_obj);
        return packet_obj;
   }
    void Driver::handle_init_packet(OS::Address from, NatNegPacket *packet, std::string gamename) {
        OS::Address private_address = OS::Address(packet->Packet.Init.localip, ntohs(packet->Packet.Init.localport));
        OS::LogText(OS::ELogLevel_Info, "[%s] Got init - version: %d, client idx: %d, cookie: %d, porttype: %d, use_gameport: %d, private: %s, game: %s", from.ToString().c_str(), packet->version, packet->Packet.Init.clientindex, ntohl(packet->cookie), packet->Packet.Init.porttype, packet->Packet.Init.usegameport, private_address.ToString().c_str(), gamename.c_str());
		packet->packettype = NN_INITACK;
		SendPacket(from, packet);

        json_t *json_obj = get_init_object(from, packet, gamename);
        char *json_data = json_dumps(json_obj, 0);

		NNRequestData req;
        req.type = ENNRequestType_SubmitJson;
		req.send_string = json_data;
		AddRequest(req);
        json_decref(json_obj);

		if (json_data)
			free((void *)json_data);


    }
}