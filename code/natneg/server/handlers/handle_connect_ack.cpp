#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include "../NNServer.h"

#include "../NNDriver.h"
#include <OS/Net/IOIfaces/BSDNetIOInterface.h>

#include <jansson.h>

namespace NN {
   json_t *get_connect_ack_object(OS::Address from, NatNegPacket *packet, std::string gamename, INetIOSocket *socket) {
		json_t *packet_obj = json_object();
		json_object_set_new(packet_obj, "from_address", json_string(from.ToString().c_str()));
        json_object_set_new(packet_obj, "driver_address", json_string(socket->address.ToString().c_str()));
        json_object_set_new(packet_obj, "hostname", json_string(OS::g_hostName));
        //
        json_object_set_new(packet_obj, "gamename", json_string(gamename.c_str()));
        json_object_set_new(packet_obj, "version", json_integer(packet->version));
        json_object_set_new(packet_obj, "type", json_string("connect_ack"));

        json_object_set_new(packet_obj, "cookie", json_integer(ntohl(packet->cookie)));
        return packet_obj;
   }
    void Driver::handle_connect_ack_packet(OS::Address from, NatNegPacket *packet, std::string gamename) {
        json_t *json_obj = get_connect_ack_object(from, packet, gamename, mp_socket);
        char *json_data = json_dumps(json_obj, 0);

		TaskScheduler<NNRequestData, TaskThreadData> *scheduler = ((NN::Server *)(getServer()))->getScheduler();
		NNRequestData req;
        req.type = ENNRequestType_SubmitJson;
		req.send_string = json_data;
		scheduler->AddRequest(ENNRequestType_SubmitJson, req);
        json_decref(json_obj);

		if (json_data)
			free((void *)json_data);

        OS::LogText(OS::ELogLevel_Info, "[%s] Got connection ACK", from.ToString().c_str());
    }
}