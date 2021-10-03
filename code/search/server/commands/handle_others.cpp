#include <OS/OpenSpy.h>
#include <OS/Buffer.h>

#include <OS/gamespy/gamespy.h>
#include <OS/gamespy/gsmsalg.h>

#include <sstream>

#include <OS/Task/TaskScheduler.h>
#include <OS/SharedTasks/tasks.h>
#include <OS/GPShared.h>

#include <server/SMPeer.h>
#include <server/SMDriver.h>
#include <server/SMServer.h>

namespace SM {
	void Peer::m_search_buddies_callback(TaskShared::WebErrorDetails error_details, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, std::map<int, GPShared::GPStatus> status_map, void *extra, INetPeer *peer) {
		std::ostringstream s;
		std::vector<OS::Profile>::iterator it = results.begin();
		s << "\\others\\";
		while(it != results.end()) {
			OS::Profile p = *it;
			OS::User u = result_users[p.userid];
			s << "\\o\\" << p.id;
			s << "\\nick\\" << p.nick;
			s << "\\first\\" << p.firstname;
			s << "\\last\\" << p.lastname;
			s << "\\email\\";
			if(u.publicmask & GP_MASK_EMAIL)
				s << u.email;
			else 
				s << Peer::mp_hidden_str;
			s << "\\uniquenick\\" << p.uniquenick;
			s << "\\namespaceid\\" << p.namespaceid;
			it++;
		}
		s << "\\odone\\";

		((Peer *)peer)->SendPacket(s.str().c_str());

		((Peer *)peer)->Delete();
	}
	void Peer::handle_others(OS::KVReader data_parser) {
		TaskShared::ProfileRequest request;
		int profileid = data_parser.GetValueInt("profileid");
		int namespaceid = data_parser.GetValueInt("namespaceid");

		request.profile_search_details.id = profileid;
		request.namespaceids.push_back(namespaceid);

		request.type = TaskShared::EProfileSearch_Buddies;
		request.extra = this;
		request.peer = this;
		IncRef();
		request.buddyCallback = Peer::m_search_buddies_callback;
		TaskScheduler<TaskShared::ProfileRequest, TaskThreadData> *scheduler = ((SM::Server *)(GetDriver()->getServer()))->GetProfileTask();
		scheduler->AddRequest(request.type, request);
	}
}