#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <algorithm>
#include <server/QRServer.h>
#include <server/QRDriver.h>
#include <OS/Net/IOIfaces/BSDNetIOInterface.h>
#include <qr/tasks/tasks.h>
#include <tasks/tasks.h>
#include <server/v2.h>
namespace QR {
    void Driver::on_v1_secure_processed(MM::MMTaskResponse response) {
		if(response.error_message != NULL) {
			response.driver->send_v1_error(response.from_address, response.error_message);		
			return;
		}
        response.driver->perform_v1_key_scan(response.query_address);
        OS::LogText(OS::ELogLevel_Info, "[%s] Server Registered", response.from_address.ToString().c_str());
    }
    void Driver::handle_v1_validate(OS::Address from_address, OS::KVReader reader) {
        std::string challenge = reader.GetValue("validate");

		TaskScheduler<MM::MMPushRequest, TaskThreadData> *scheduler = ((QR::Server *)(getServer()))->getScheduler();
		MM::MMPushRequest req;

		req.gamename = challenge;
        req.callback = on_v1_secure_processed;
        req.from_address = from_address;
        req.v2_instance_key = 0;
        req.driver = this;
        req.version = 1;

		OS::LogText(OS::ELogLevel_Info, "[%s] Got challenge response: %s", from_address.ToString().c_str(), challenge.c_str());
		req.type = MM::EMMPushRequestType_ValidateServer;
		scheduler->AddRequest(req.type, req);
    }
}
    