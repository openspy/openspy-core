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
    void Driver::on_got_v2_challenge_response(MM::MMTaskResponse response) {
        if(response.error_message != NULL) {
            response.driver->send_v2_error(response.from_address, response.v2_instance_key, 1, response.error_message);		
            return;
        }
		OS::Buffer buffer;
		buffer.WriteByte(QR_MAGIC_1);
		buffer.WriteByte(QR_MAGIC_2);

		buffer.WriteByte(PACKET_CLIENT_REGISTERED);
		buffer.WriteInt(response.v2_instance_key);
		response.driver->SendPacket(response.from_address, buffer);
        OS::LogText(OS::ELogLevel_Info, "[%s] Server Registered", response.from_address.ToString().c_str());
    }
    void Driver::handle_v2_challenge(OS::Address from_address, uint8_t *instance_key, OS::Buffer &buffer) {
        std::string challenge = buffer.ReadNTS();

		TaskScheduler<MM::MMPushRequest, TaskThreadData> *scheduler = ((QR::Server *)(getServer()))->getScheduler();
		MM::MMPushRequest req;

		req.gamename = challenge;
        req.callback = on_got_v2_challenge_response;
        req.from_address = from_address;
        req.v2_instance_key = *(uint32_t *)instance_key;
        req.driver = this;
        req.version = 2;

		req.type = MM::EMMPushRequestType_ValidateServer;
		scheduler->AddRequest(req.type, req);
    }
}