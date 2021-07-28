#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <server/QRServer.h>
#include <server/QRDriver.h>
#include <OS/Net/IOIfaces/BSDNetIOInterface.h>
#include <qr/tasks/tasks.h>
#include <tasks/tasks.h>
#include <server/v2.h>
namespace QR {
    void Driver::on_keepalive_processed(MM::MMTaskResponse response) {
		if(response.error_message != NULL) {
			//response.driver->send_v2_error(response.from_address, response.v2_instance_key, 1, response.error_message);		
			return;
		} else {
            OS::Buffer send_buffer;
            struct timeval current_time;
            send_buffer.WriteByte(QR_MAGIC_1);
            send_buffer.WriteByte(QR_MAGIC_2);
            send_buffer.WriteByte(PACKET_KEEPALIVE);
            send_buffer.WriteInt(response.v2_instance_key);
            send_buffer.WriteInt(current_time.tv_sec);
            response.driver->SendPacket(response.from_address, send_buffer);			
		}
    }
    void Driver::handle_v2_keepalive(OS::Address from_address, uint8_t *instance_key, OS::Buffer &buffer) {
        //send response
        OS::LogText(OS::ELogLevel_Info, "[%s] Got keepalive", from_address.ToString().c_str());


        TaskScheduler<MM::MMPushRequest, TaskThreadData> *scheduler = ((QR::Server *)(getServer()))->getScheduler();
        MM::MMPushRequest req;

        req.callback = on_keepalive_processed;
        req.from_address = from_address;
        req.v2_instance_key = *(uint32_t *)instance_key;
        req.driver = this;
        req.version = 2;

        
        req.type = MM::EMMPushRequestType_Keepalive;
        scheduler->AddRequest(req.type, req);
    }
}