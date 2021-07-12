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

	void Peer::m_search_callback(TaskShared::WebErrorDetails error_details, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer) {
		std::ostringstream s;
		std::vector<OS::Profile>::iterator it = results.begin();
		while(it != results.end()) {
			OS::Profile p = *it;
			OS::User u = result_users[p.userid];
			s << "\\bsr\\" << p.id;
			s << "\\nick\\" << p.nick;
			s << "\\firstname\\" << p.firstname;
			s << "\\lastname\\" << p.lastname;
			s << "\\email\\";
			if(u.publicmask & GP_MASK_EMAIL || ((int)extra) != 0)
				s << u.email;
			else 
				s << Peer::mp_hidden_str;
			s << "\\uniquenick\\" << p.uniquenick;
			s << "\\namespaceid\\" << p.namespaceid;
			it++;
		}
		s << "\\bsrdone\\";
		((Peer *)peer)->SendPacket(s.str().c_str());

		((Peer *)peer)->Delete();
	}
	void Peer::handle_search(OS::KVReader data_parser) {
		TaskShared::ProfileRequest request;
		
		request.profile_search_details.id = 0;
		if(data_parser.HasKey("email")) {
			request.user_search_details.email = data_parser.GetValue("email");
		}
		if(data_parser.HasKey("nick")) {
			request.profile_search_details.nick = data_parser.GetValue("nick");
		}
		if(data_parser.HasKey("uniquenick")) {
			request.profile_search_details.uniquenick = data_parser.GetValue("uniquenick");
		}
		if(data_parser.HasKey("firstname")) {
			request.profile_search_details.firstname = data_parser.GetValue("firstname");
		}
		if (data_parser.HasKey("lastname")) {
			request.profile_search_details.lastname = data_parser.GetValue("lastname");
		}
		
		if (data_parser.HasKey("namespaceid")) {
			int namespace_id = data_parser.GetValueInt("namespaceid");
			request.namespaceids.push_back(namespace_id);
		}

		if (data_parser.HasKey("namespaceids")) {
			std::stringstream ns_ss(data_parser.GetValue("namespaceids"));
			std::string ns_token;
			while (std::getline(ns_ss, ns_token, ',')) {
				request.namespaceids.push_back(atoi(ns_token.c_str()));
			}
		}

		if (data_parser.HasKey("partnerid")) {
			request.user_search_details.partnercode = data_parser.GetValueInt("partnerid");
		}

		request.type = TaskShared::EProfileSearch_Profiles;
		request.peer = this;
		request.extra = (void *)(request.user_search_details.email.length() > 0);
		IncRef();
		request.callback = Peer::m_search_callback;
		TaskScheduler<TaskShared::ProfileRequest, TaskThreadData> *scheduler = ((SM::Server *)(GetDriver()->getServer()))->GetProfileTask();
		scheduler->AddRequest(request.type, request);
	}
}