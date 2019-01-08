#include <GP/server/GPPeer.h>
#include <GP/server/GPDriver.h>
#include <GP/server/GPServer.h>
#include <OS/OpenSpy.h>
#include <OS/Search/Profile.h>

#include <OS/Search/User.h>

#include <OS/Buffer.h>
#include <OS/KVReader.h>
#include <sstream>
#include <algorithm>

#include <OS/gamespy/gamespy.h>
#include <tasks/tasks.h>

namespace GP {
	void Peer::handle_authadd(OS::KVReader data_parser) {
		if (data_parser.HasKey("fromprofileid")) {
			int fromprofileid = data_parser.GetValueInt("fromprofileid");
			TaskScheduler<GP::GPBackendRedisRequest, TaskThreadData> *scheduler = ((GP::Server *)(GetDriver()->getServer()))->GetGPTask();
			GPBackendRedisRequest req;
			req.type = EGPRedisRequestType_AuthorizeAdd;
			req.peer = this;
			req.peer->IncRef();
			req.ToFromData.from_profileid = fromprofileid;
			req.ToFromData.to_profileid = m_profile.id;
			scheduler->AddRequest(req.type, req);
		} else {
			send_error(GPShared::GP_PARSE);
			return;
		}
	}
	void Peer::send_authorize_add(int profileid, bool silent) {
		if (!silent) {
			std::ostringstream s;
			s << "\\addbuddyresponse\\" << GPI_BM_REQUEST; //the addbuddy response might be implemented wrong
			s << "\\newprofileid\\" << profileid;
			s << "\\confirmation\\d41d8cd98f00b204e9800998ecf8427e"; //temp until calculation fixed
			SendPacket((const uint8_t *)s.str().c_str(), s.str().length());

			s.str("");
			s << "\\bm\\" << GPI_BM_AUTH;
			s << "\\f\\" << profileid;
			s << "\\msg\\" << "I have authorized your request to add me to your list";
			s << "|signed|d41d8cd98f00b204e9800998ecf8427e"; //temp until calculation fixed
			SendPacket((const uint8_t *)s.str().c_str(), s.str().length());
		}

		m_buddies[profileid] = GPShared::gp_default_status;

	}
}