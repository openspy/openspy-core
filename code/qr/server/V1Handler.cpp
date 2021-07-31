#include <stdio.h>
#include <stdlib.h>
#include  <algorithm>
#include "QRServer.h"
#include "QRDriver.h"
#include <OS/Net/IOIfaces/BSDNetIOInterface.h>
#include "v2.h"
namespace QR {
    void Driver::handle_v1_packet(INetIODatagram &packet) {
        std::string input = std::string((char *)packet.buffer.GetHead(), packet.buffer.readRemaining());        
        packet.buffer.SetReadCursor(packet.buffer.readRemaining());

        OS::KVReader data_parser = input;

        std::string message_type = data_parser.GetKeyByIdx(0);
        printf("input is: %s\n", message_type.c_str());

        if(message_type.compare("heartbeat") == 0) {
            handle_v1_heartbeat(packet.address, data_parser);
        } else if(message_type.compare("validate") == 0) {
            handle_v1_validate(packet.address, data_parser);
        } else if (message_type.compare("echo") == 0) {
            handle_v1_echo(packet.address, data_parser);
        } else {
            //read heartbeat server info
            handle_v1_heartbeat_data(packet.address, data_parser);
        }
        
        
    }
    void Driver::handle_v1_packet(OS::KVReader data_parser) {
        printf("handle_v1_packet\n");
    }
}