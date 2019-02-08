#include <OS/OpenSpy.h>
#include <OS/Search/Profile.h>

#include <OS/Search/User.h>

#include <OS/Buffer.h>
#include <OS/KVReader.h>
#include <sstream>
#include <algorithm>

#include <OS/gamespy/gamespy.h>
#include <OS/SharedTasks/tasks.h>
#include <tasks/tasks.h>

#include <GP/server/GPPeer.h>
#include <GP/server/GPDriver.h>
#include <GP/server/GPServer.h>

namespace GP {
	void Peer::send_list_status() {
		if (!m_got_blocks || !m_got_buddies) return;
		std::vector<int> all_profileids;
		std::vector<int>::iterator it = m_blocks.begin();
		while (it != m_blocks.end()) {
			all_profileids.push_back((*it));
			it++;
		}

		it = m_blocked_by.begin();
		while (it != m_blocked_by.end()) {
			all_profileids.push_back((*it));
			it++;
		}

		std::map<int, GPShared::GPStatus>::iterator it2 = m_buddies.begin();
		while (it2 != m_buddies.end()) {
			all_profileids.push_back((*it2).first);
			it2++;
		}

		it = all_profileids.begin();
		while (it != all_profileids.end()) {
			int pid = *it;
			if (m_buddies.find(pid) != m_buddies.end()) {
				inform_status_update(pid, m_buddies[pid], true);
			}
			else {
				inform_status_update(pid, GPShared::gp_default_status, true);
			}
			it++;
		}
	}
    void Peer::m_block_list_lookup_callback(TaskShared::WebErrorDetails error_details, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, std::map<int, GPShared::GPStatus> status_map, void *extra, INetPeer *peer) {
		std::ostringstream s;
		std::string str;
		std::vector<OS::Profile>::iterator it = results.begin();
		((GP::Peer *)peer)->mp_mutex->lock();
		if(results.size() > 0) {
			s << "\\blk\\" << results.size();
			s << "\\list\\";
			while(it != results.end()) {
				OS::Profile p = (*it);
				s << p.id << ",";
				((GP::Peer *)peer)->m_blocks.push_back(p.id);
				it++;
			}
			str = s.str();
			str = str.substr(0, str.size()-1);
			((GP::Peer *)peer)->SendPacket((const uint8_t *)str.c_str(),str.length());
		}
		((GP::Peer *)peer)->m_got_blocks = true;
		((GP::Peer *)peer)->mp_mutex->unlock();
		((GP::Peer *)peer)->send_list_status();
    }
    void Peer::m_buddy_list_lookup_callback(TaskShared::WebErrorDetails error_details, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, std::map<int, GPShared::GPStatus> status_map, void *extra, INetPeer *peer) {
		std::ostringstream s;
		std::string str;
		std::vector<OS::Profile>::iterator it = results.begin();
		((GP::Peer *)peer)->mp_mutex->lock();
		if(results.size() > 0) {
			s << "\\bdy\\" << results.size();
			s << "\\list\\";
			while(it != results.end()) {
				OS::Profile p = (*it);
				s << p.id << ",";

				GPShared::GPStatus status = status_map[p.id];
				if (status.status == GPShared::GP_OFFLINE) {
					status = GPShared::gp_default_status;
				}
				((GP::Peer *)peer)->m_buddies[p.id] = status;

				it++;
			}
			str = s.str();
			str = str.substr(0, str.size()-1);
			((GP::Peer *)peer)->SendPacket((const uint8_t *)str.c_str(),str.length());
		}
		((GP::Peer *)peer)->m_got_buddies = true;
		((GP::Peer *)peer)->mp_mutex->unlock();
		((GP::Peer *)peer)->send_list_status();
    }

	void Peer::send_buddies() {
		TaskShared::ProfileRequest request;
		request.profile_search_details.id = m_profile.id;
		request.peer = this;
		request.peer->IncRef();
		request.type = TaskShared::EProfileSearch_Buddies;
		request.buddyCallback = Peer::m_buddy_list_lookup_callback;
		TaskScheduler<TaskShared::ProfileRequest, TaskThreadData> *scheduler = ((GP::Server *)(GetDriver()->getServer()))->GetProfileTask();
		scheduler->AddRequest(request.type, request);
	}
	void Peer::send_blocks() {
		TaskShared::ProfileRequest request;
		request.profile_search_details.id = m_profile.id;
		request.peer = this;
		request.peer->IncRef();
		request.type = TaskShared::EProfileSearch_Blocks;
		request.buddyCallback = Peer::m_block_list_lookup_callback;
		TaskScheduler<TaskShared::ProfileRequest, TaskThreadData> *scheduler = ((GP::Server *)(GetDriver()->getServer()))->GetProfileTask();
		scheduler->AddRequest(request.type, request);
	}
}