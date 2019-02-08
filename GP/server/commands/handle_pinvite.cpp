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
	void Peer::handle_pinvite(OS::KVReader data_parser) {
		//profileid\10000\productid\1
		std::ostringstream s;
		//OS::KVReader data_parser = OS::KVReader(std::string(data));
		int profileid = 0;
		if (data_parser.HasKey("profileid")) {
			profileid = data_parser.GetValueInt("profileid");
		}
		int productid = 0;
		if (data_parser.HasKey("productid")) {
			productid = data_parser.GetValueInt("productid");
		}

		s << "|p|" << productid;
		s << "|l|" << m_status.location_str;
		//s << "|signed|d41d8cd98f00b204e9800998ecf8427e"; //temp until calculation fixed
		//GPBackend::GPBackendRedisTask::SendMessage(this, profileid, GPI_BM_INVITE, s.str().c_str());

		TaskScheduler<GP::GPBackendRedisRequest, TaskThreadData> *scheduler = ((GP::Server *)(GetDriver()->getServer()))->GetGPTask();
		GPBackendRedisRequest req;
		req.type = EGPRedisRequestType_BuddyMessage;
		req.peer = this;
		req.peer->IncRef();
		req.BuddyMessage.to_profileid = profileid;
		req.BuddyMessage.type = GPI_BM_INVITE;
		req.BuddyMessage.message = s.str();	
		
		
		scheduler->AddRequest(req.type, req);
	}
}