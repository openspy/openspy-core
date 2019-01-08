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
	void Peer::handle_addbuddy(OS::KVReader data_parser) {
		//OS::KVReader data_parser = OS::KVReader(std::string(data));
		int newprofileid = 0;
		std::string reason;
		if (data_parser.HasKey("reason")) {
			reason = data_parser.GetValue("reason");
		}

		if (data_parser.HasKey("newprofileid")) {
			newprofileid = data_parser.GetValueInt("newprofileid");
		}
		else {
			send_error(GPShared::GP_PARSE);
			return;
		}

		mp_mutex->lock();
		if(m_buddies.find(newprofileid) != m_buddies.end()) {
			mp_mutex->unlock();
			send_error(GP_ADDBUDDY_ALREADY_BUDDY);
			return;
		} else if(std::find(m_blocks.begin(),m_blocks.end(), newprofileid) != m_blocks.end()) {
			mp_mutex->unlock();
			send_error(GP_ADDBUDDY_IS_ON_BLOCKLIST);
			return;
		}
		mp_mutex->unlock();

		TaskScheduler<GP::GPBackendRedisRequest, TaskThreadData> *scheduler = ((GP::Server *)(GetDriver()->getServer()))->GetGPTask();
		GPBackendRedisRequest req;
		req.type = EGPRedisRequestType_AddBuddy;
		req.peer = this;
		req.peer->IncRef();
		req.BuddyRequest.from_profileid = m_profile.id;
		req.BuddyRequest.to_profileid = newprofileid;
		req.BuddyRequest.reason = reason;
		req.StatusInfo = m_status;
		scheduler->AddRequest(req.type, req);
	}
	void Peer::send_add_buddy_request(int from_profileid, const char *reason) {
		////\bm\1\f\157928340\msg\I have authorized your request to add me to your list\final
		std::ostringstream s;
		s << "\\bm\\" << GPI_BM_REQUEST;
		s << "\\f\\" << from_profileid;
		s << "\\msg\\" << reason;
		s << "|signed|d41d8cd98f00b204e9800998ecf8427e"; //temp until calculation fixed

		SendPacket((const uint8_t *)s.str().c_str(),s.str().length());
	}
}