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
	void Peer::handle_nicks(OS::KVReader data_parser) {
		TaskShared::ProfileRequest request;
		request.type = TaskShared::EProfileSearch_Profiles;
		if (data_parser.HasKey("userid")) {
			request.user_search_details.id = data_parser.GetValueInt("userid");
		}

		if (data_parser.HasKey("partnercode")) {
			request.user_search_details.partnercode = data_parser.GetValueInt("partnercode");
		}
		else if (data_parser.HasKey("partnerid")) {
			request.user_search_details.partnercode = data_parser.GetValueInt("partnerid");
		}

		if (data_parser.HasKey("email")) {
			request.user_search_details.email = data_parser.GetValue("email");
		}

		if (data_parser.HasKey("namespaceid")) {
			request.profile_search_details.namespaceid = data_parser.GetValueInt("namespaceid");
		}

		std::string password;

		if (data_parser.HasKey("passenc")) {
			password = data_parser.GetValue("passenc");
			int passlen = (int)password.length();
			char *dpass = (char *)base64_decode((uint8_t *)password.c_str(), &passlen);
			passlen = gspassenc((uint8_t *)dpass);
			password = dpass;
			if (dpass)
				free((void *)dpass);
		}
		else if (data_parser.HasKey("pass")) {
			password = data_parser.GetValue("pass");
		}
		else if (data_parser.HasKey("password")) {
			password = data_parser.GetValue("password");
		}
		else {
			send_error(GPShared::GP_PARSE);
			return;
		}

		request.user_search_details.password = password;

		request.extra = this;
		request.peer = this;
		IncRef();
		request.callback = Peer::m_nicks_cb;
		TaskScheduler<TaskShared::ProfileRequest, TaskThreadData> *scheduler = ((SM::Server *)(GetDriver()->getServer()))->GetProfileTask();
		scheduler->AddRequest(request.type, request);
	}
	void Peer::m_nicks_cb(TaskShared::WebErrorDetails error_details, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer) {
		std::ostringstream s;

		s << "\\nr\\" << results.size();

		std::vector<OS::Profile>::iterator it = results.begin();
		while (it != results.end()) {
			OS::Profile p = *it;
			if (p.uniquenick.length())
				s << "\\uniquenick\\" << p.uniquenick;
			else if(p.nick.length())
				s << "\\nick\\" << p.nick;
			it++;
		}

		s << "\\ndone\\";

		((Peer *)peer)->SendPacket(s.str().c_str());
	}
}