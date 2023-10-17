#include <stdio.h>
#include <stdlib.h>
#include  <algorithm>
#include "QRServer.h"
#include "QRDriver.h"
#include "v2.h"
namespace QR {
    void Driver::handle_v1_packet(OS::Address address, OS::Buffer buffer) {
        std::string input = std::string((char *)buffer.GetHead(), buffer.readRemaining());        
        buffer.SetReadCursor(buffer.readRemaining());

        OS::KVReader data_parser = input;


        std::string message_type = data_parser.GetKeyByIdx(0);

        if(message_type.compare("heartbeat") == 0) {
            handle_v1_heartbeat(address, data_parser);
        } else if(message_type.compare("validate") == 0) {
            handle_v1_validate(address, data_parser);
        } else if (message_type.compare("echo") == 0) {
            handle_v1_echo(address, data_parser);
        } else {
            //read heartbeat server info
            handle_v1_heartbeat_data(address, data_parser);
        }
    }
}