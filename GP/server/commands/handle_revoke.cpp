#include <OS/OpenSpy.h>
#include <OS/Search/Profile.h>

#include <OS/Search/User.h>

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
	void Peer::handle_revoke(OS::KVReader data_parser) {
		if (data_parser.HasKey("profileid")) {
			int delprofileid = data_parser.GetValueInt("profileid");
			TaskScheduler<GP::GPBackendRedisRequest, TaskThreadData> *scheduler = ((GP::Server *)(GetDriver()->getServer()))->GetGPTask();
			GPBackendRedisRequest req;
			req.type = EGPRedisRequestType_DelBuddy;
			req.peer = this;
			req.peer->IncRef();
			req.ToFromData.to_profileid = m_profile.id;
			req.ToFromData.from_profileid = delprofileid;
			scheduler->AddRequest(req.type, req);
		} else {
			send_error(GPShared::GP_PARSE);
			return;
		}
	}
	void Peer::send_revoke_message(int from_profileid, int date_unix_timestamp) {
		m_buddies.erase(from_profileid);
		inform_status_update(from_profileid, GPShared::gp_default_status, true);
		
		std::ostringstream s;
		s << "\\bm\\" << GPI_BM_REVOKE;
		s << "\\f\\" << from_profileid;
		s << "\\msg\\I have revoked you from my list.";
		s << "|signed|d41d8cd98f00b204e9800998ecf8427e"; //temp until calculation fixed
		if(date_unix_timestamp != 0)
			s << "\\date\\" << date_unix_timestamp;
		SendPacket((const uint8_t *)s.str().c_str(),s.str().length());

	}
}