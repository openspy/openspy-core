


#include <tasks/tasks.h>
#include <sstream>
#include "../server/v2.h"

#include <OS/gamespy/gsmsalg.h>

namespace MM {
    bool PerformKeepalive(MMPushRequest request, TaskThreadData *thread_data) {
        MMTaskResponse response;
        response.v2_instance_key = request.v2_instance_key;
        response.driver = request.driver;
        response.from_address = request.from_address;
        response.challenge = request.gamename; //copy echo key

        std::string server_key = GetServerKeyBy_InstanceKey_Address(thread_data, request.v2_instance_key, request.from_address);
        if(server_key.length() == 0) {
            response.error_message = "No server registered";
        } else {
            WriteLastHeartbeatTime(thread_data, server_key, request.from_address, request.v2_instance_key);
        }        
        
        request.callback(response);

        return true;
    }

}