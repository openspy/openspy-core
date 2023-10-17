#include <OS/OpenSpy.h>

#include <OS/Buffer.h>
#include <OS/KVReader.h>
#include <sstream>
#include <algorithm>

#include <OS/gamespy/gamespy.h>
#include <tasks/tasks.h>


#include <server/Driver.h>
#include <server/Server.h>
#include <server/Peer.h>

namespace Peerchat {
    void Peer::handle_cdkey(std::vector<std::string> data_parser) {
			std::ostringstream ss;
			ss << 1 << " Success";
			send_numeric(706, ss.str(), true);
    }
}