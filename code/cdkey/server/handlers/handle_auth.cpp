#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include "../CDKeyServer.h"
#include "../CDKeyDriver.h"
#include <OS/Net/IOIfaces/BSDNetIOInterface.h>

#include <sstream>

namespace CDKey {
	void Driver::handle_auth_packet(OS::Address from, OS::KVReader data_parser) {
        std::ostringstream ss;
        std::string cdkey = data_parser.GetValue("resp");
        if(cdkey.length() > 32) {
            cdkey = cdkey.substr(0, 32);
        }
        ss << "\\uok\\\\cd\\" << cdkey << "\\skey\\" << data_parser.GetValueInt("skey");

        SendPacket(from, ss.str());
	}
}