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
	void Peer::handle_status(OS::KVReader data_parser) {
		if (data_parser.HasKey("status")) {
			m_status.status = (GPEnum)data_parser.GetValueInt("status");
		}
		if (data_parser.HasKey("statstring")) {
			m_status.status_str = data_parser.GetValue("statstring");
		}
		if (data_parser.HasKey("locstring")) {
			m_status.location_str = data_parser.GetValue("locstring");
		}

		GPBackendRedisRequest req;
		req.type = EGPRedisRequestType_UpdateStatus;
		req.peer = this;
		req.peer->IncRef();
		req.StatusInfo = m_status;
		AddGPTaskRequest(req);
	}
	void Peer::inform_status_update(int profileid, GPStatus status, bool no_update) {
		std::ostringstream ss;
		//mp_mutex->lock();
		bool is_blocked = std::find(m_blocks.begin() ,m_blocks.end(), profileid) != m_blocks.end() || std::find(m_blocked_by.begin(), m_blocked_by.end(), profileid) != m_blocked_by.end();
		if(m_buddies.find(profileid) != m_buddies.end() || is_blocked) {

			if(!no_update)
				m_buddies[profileid] = status;

			if (is_blocked) {
				m_blocks.push_back(profileid);
			}

			if(status.status == GPShared::GP_OFFLINE || is_blocked) {
				status = GPShared::gp_default_status;
			}

			ss << "\\bm\\" << GPI_BM_STATUS;

			ss << "\\f\\" << profileid;

			ss << "\\msg\\";

			ss << "|s|" << status.status;
			if(status.status_str[0] != 0)
				ss << "|ss|" << status.status_str;

			if(status.location_str[0] != 0)
				ss << "|ls|" << status.location_str;

			if(status.address.ip != 0) {
				ss << "|ip|" << htonl(status.address.ip);
				ss << "|p|" << status.address.GetPort();
			}

			if(status.quiet_flags != GP_SILENCE_NONE) {
				ss << "|qm|" << status.quiet_flags;
			}
			SendPacket((const uint8_t *)ss.str().c_str(),ss.str().length());
		}
		//mp_mutex->unlock();
	}
	void Peer::m_session_handle_update(TaskShared::WebErrorDetails error_details, std::map<int, GPShared::GPStatus> results, void *extra, INetPeer *peer)  {
		std::map<int, GPShared::GPStatus>::iterator it = results.begin();
		while(it != results.end()) {
			std::pair<int, GPShared::GPStatus> p = *it;
			((Peer *)peer)->inform_status_update(p.first, p.second, false);
			it++;
		}
	}
}