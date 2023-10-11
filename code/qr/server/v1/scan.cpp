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
    void Driver::perform_v1_key_scan(OS::Address from_address) {
        std::stringstream ss;
        ss << "\\status\\";

        std::string message = ss.str();

        OS::Buffer buffer;
        buffer.WriteBuffer(message.c_str(),message.length());
        SendUDPPacket(from_address, buffer);
    }
}
    