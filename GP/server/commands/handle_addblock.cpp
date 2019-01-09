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
	void Peer::handle_addblock(OS::KVReader data_parser) {
		if (data_parser.HasKey("profileid")) {
			int profileid = data_parser.GetValueInt("profileid");
			TaskScheduler<GP::GPBackendRedisRequest, TaskThreadData> *scheduler = ((GP::Server *)(GetDriver()->getServer()))->GetGPTask();
			GPBackendRedisRequest req;
			req.type = EGPRedisRequestType_AddBlock;
			req.peer = this;
			req.peer->IncRef();
			req.ToFromData.to_profileid = profileid;
			req.ToFromData.from_profileid = m_profile.id;
			scheduler->AddRequest(req.type, req);
		} else {
			send_error(GPShared::GP_PARSE);
			return;
		}
		
	}
}