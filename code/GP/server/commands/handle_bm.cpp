#include <OS/OpenSpy.h>

#include <OS/Buffer.h>
#include <OS/KVReader.h>
#include <sstream>
#include <algorithm>

#include <OS/gamespy/gamespy.h>
#include <tasks/tasks.h>

#include <GP/server/GPPeer.h>
#include <GP/server/GPDriver.h>
#include <GP/server/GPServer.h>

namespace GP {
	void Peer::handle_bm(OS::KVReader data_parser) {
		//OS::KVReader data_parser = OS::KVReader(std::string(data));
		if (data_parser.HasKey("t") && data_parser.HasKey("bm") && data_parser.HasKey("msg")) {
			int to_profileid = data_parser.GetValueInt("t");
			int msg_type = data_parser.GetValueInt("bm");
			std::string msg = data_parser.GetValue("msg");

			if (m_buddies.find(to_profileid) == m_buddies.end()) {
				send_error(GPShared::GP_BM_NOT_BUDDY);
				return;
			}

			if (m_buddies[to_profileid].status == GP_OFFLINE) {
				send_error(GPShared::GP_BM_BUDDY_OFFLINE);
				return;
			}
			
			switch (msg_type) {
			case GPI_BM_MESSAGE:
			case GPI_BM_UTM:
			case GPI_BM_PING:
			case GPI_BM_PONG:
				break;
			default:
				return;
			}

			GPBackendRedisRequest req;
			req.type = EGPRedisRequestType_BuddyMessage;
			req.peer = this;
			req.peer->IncRef();
			req.BuddyMessage.type = msg_type;
			req.BuddyMessage.to_profileid = to_profileid;
			req.BuddyMessage.message = msg;
			AddGPTaskRequest(req);
		}
		else {
			send_error(GPShared::GP_PARSE);
		}
	}
	void Peer::send_buddy_message(char type, int from_profileid, int timestamp, const char *msg) {
		std::ostringstream s;
		s << "\\bm\\" << ((int)type);
		s << "\\f\\" << from_profileid;
		if(timestamp != 0)
			s << "\\date\\" << timestamp;
		s << "\\msg\\" << msg;
		SendPacket((const uint8_t *)s.str().c_str(),s.str().length());
	}
}