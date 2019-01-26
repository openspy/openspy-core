#include <server/GSServer.h>
#include <server/GSPeer.h>
#include <server/GSDriver.h>
#include <OS/OpenSpy.h>
#include <OS/Buffer.h>
#include <OS/Search/Profile.h>

#include <OS/Search/User.h>

#include <stddef.h>
#include <sstream>
#include <algorithm>

#include <server/tasks/tasks.h>

#include <OS/GPShared.h>

#include <map>
#include <utility>

using namespace GPShared;

namespace GS {
    void Peer::handle_getpid(OS::KVReader data_parser) {
		/*
				Send error response until implemented
		*/
		int operation_id = data_parser.GetValueInt("lid");
		//int pid = data_parser.GetValueInt("pid");
		std::ostringstream ss;
		ss << "\\getpidr\\-1\\lid\\" << operation_id;
		SendPacket(ss.str());
	}
}