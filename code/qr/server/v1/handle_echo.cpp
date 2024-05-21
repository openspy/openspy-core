#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <algorithm>
#include <server/QRServer.h>
#include <server/QRDriver.h>
#include <tasks/tasks.h>
#include <server/v2.h>
namespace QR {
	void Driver::on_v1_echo_processed(MM::MMTaskResponse response) {
		if(response.error_message != NULL) {
			//response.driver->send_v1_error(response.from_address, response.error_message);		
			return;
		}
		std::stringstream ss;
		ss << "\\echo\\" << response.challenge;

		std::string message = ss.str();

		OS::Buffer buffer;
		buffer.WriteBuffer(message.c_str(),message.length());
		response.driver->SendUDPPacket(response.from_address, buffer);
	}
	void Driver::handle_v1_echo(OS::Address from_address, OS::KVReader reader) {
		OS::LogText(OS::ELogLevel_Info, "[%s] Got echo", from_address.ToString().c_str());

		MM::MMPushRequest req;

		req.callback = on_v1_echo_processed;
		req.from_address = from_address;
		req.v2_instance_key = 0;
		req.driver = this;
		req.version = 1;
		req.gamename = reader.GetValueByIdx(0); //get echo string

		
		req.type = MM::EMMPushRequestType_Keepalive;
		AddRequest(req);
	}
}
	