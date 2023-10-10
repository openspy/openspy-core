


#include <tasks/tasks.h>
#include <sstream>
#include "../server/v2.h"
#include <server/QRDriver.h>
#include <OS/gamespy/gsmsalg.h>

namespace MM {
    bool PerformClientMessageAck(MMPushRequest request, TaskThreadData *thread_data) {
        MMTaskResponse response;
        response.v2_instance_key = request.v2_instance_key;
        response.driver = request.driver;
        response.from_address = request.from_address;

        json_t *send_obj = json_object();   

        json_object_set_new(send_obj, "instance_key", json_integer(request.v2_instance_key));
        json_object_set_new(send_obj, "version", json_integer(2));
        json_object_set_new(send_obj, "identifier", json_integer(request.server.id));

        json_object_set_new(send_obj, "hostname", json_string(OS::g_hostName));
        json_object_set_new(send_obj, "driver_address", json_string(request.driver->getListenerSocket()->address.ToString().c_str()));
        json_object_set_new(send_obj, "from_address", json_string(request.from_address.ToString().c_str()));

        char *json_dump = json_dumps(send_obj, 0);


        sendAMQPMessage(mm_channel_exchange, mm_server_client_acks_routingkey, json_dump);

		if (json_dump) {
			free((void *)json_dump);
		}

        json_decref(send_obj);
        
        if(request.callback)
            request.callback(response);

        return true;
    }

}