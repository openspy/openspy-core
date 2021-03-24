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
	void Peer::handle_searchunique(OS::KVReader data_parser) {
		TaskShared::ProfileRequest request;

		request.profile_search_details.id = 0;
		if (data_parser.HasKey("email")) {
			request.user_search_details.email = data_parser.GetValue("email");
		}
		if (data_parser.HasKey("nick")) {
			request.profile_search_details.nick = data_parser.GetValue("nick");
		}
		if (data_parser.HasKey("uniquenick")) {
			request.profile_search_details.uniquenick = data_parser.GetValue("uniquenick");
		}
		if (data_parser.HasKey("firstname")) {
			request.profile_search_details.firstname = data_parser.GetValue("firstname");
		}
		if (data_parser.HasKey("lastname")) {
			request.profile_search_details.lastname = data_parser.GetValue("lastname");
		}

		int namespace_id = data_parser.GetValueInt("namespaceid");
		if (namespace_id != 0) {
			request.namespaceids.push_back(namespace_id);
		}

		if (data_parser.HasKey("partnerid")) {
			request.user_search_details.partnercode = data_parser.GetValueInt("partnerid");
		}
		else if (data_parser.HasKey("partnercode")) {
			request.user_search_details.partnercode = data_parser.GetValueInt("partnercode");
		}
		

		if (data_parser.HasKey("namespaceids")) {
			std::stringstream ns_ss(data_parser.GetValue("namespaceids"));
			std::string ns_token;
			while (std::getline(ns_ss, ns_token, ',')) {
				request.namespaceids.push_back(atoi(ns_token.c_str()));
			}
		}

		if (data_parser.HasKey("namespaces")) {
			std::stringstream ns_ss(data_parser.GetValue("namespaces"));
			std::string ns_token;
			while (std::getline(ns_ss, ns_token, ',')) {
				request.namespaceids.push_back(atoi(ns_token.c_str()));
			}
		}

		request.type = TaskShared::EProfileSearch_Profiles;
		request.extra = this;
		request.peer = this;
		IncRef();
		request.callback = Peer::m_search_callback;
		TaskScheduler<TaskShared::ProfileRequest, TaskThreadData> *scheduler = ((SM::Server *)(GetDriver()->getServer()))->GetProfileTask();
		scheduler->AddRequest(request.type, request);
	}
}