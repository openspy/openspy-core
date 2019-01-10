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
	void Peer::handle_delbuddy(OS::KVReader data_parser) {
		if (data_parser.HasKey("delprofileid")) {
			int delprofileid = data_parser.GetValueInt("delprofileid");
			if (m_buddies.find(delprofileid) != m_buddies.end()) {
				TaskScheduler<GP::GPBackendRedisRequest, TaskThreadData> *scheduler = ((GP::Server *)(GetDriver()->getServer()))->GetGPTask();
				GPBackendRedisRequest req;
				req.type = EGPRedisRequestType_DelBuddy;
				req.peer = (GP::Peer *)this;
				req.peer->IncRef();
				req.ToFromData.from_profileid = m_profile.id;
				req.ToFromData.to_profileid = delprofileid;
				m_buddies.erase(delprofileid);
				scheduler->AddRequest(req.type, req);
			}
		}
		else {
			send_error(GPShared::GP_PARSE);
			return;
		}
	}

}