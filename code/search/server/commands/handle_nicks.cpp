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

#include <OS/GPShared.h>

namespace SM {
	void Peer::m_nicks_auth_cb(bool success, OS::User user, OS::Profile profile, TaskShared::AuthData auth_data, void* extra, INetPeer* peer) {
		if (success) {
			int namespaceid = (int)extra;
			TaskShared::ProfileRequest request;
			request.type = TaskShared::EProfileSearch_Profiles;
			request.peer = peer;
			request.user_search_details = user;
			request.profile_search_details.namespaceid = namespaceid;
			peer->IncRef();
			request.callback = Peer::m_nicks_cb;
			TaskScheduler<TaskShared::ProfileRequest, TaskThreadData>* scheduler = ((SM::Server*)(peer->GetDriver()->getServer()))->GetProfileTask();
			scheduler->AddRequest(request.type, request);

		}
		else {
			std::ostringstream s;
			GPShared::GPErrorCode code = GPShared::GP_NETWORK;
			s << "\\nr\\";
			switch (auth_data.error_details.response_code) {
				case TaskShared::WebErrorCode_NoSuchUser:
					code = (GPShared::GP_LOGIN_BAD_EMAIL);
					break;
				case TaskShared::WebErrorCode_AuthInvalidCredentials:
					code = (GPShared::GP_LOGIN_BAD_PASSWORD);
					break;
				case TaskShared::WebErrorCode_NickInvalid:
					code = (GPShared::GP_LOGIN_BAD_PROFILE);
					break;
				case TaskShared::WebErrorCode_UniqueNickInvalid:
					code = (GPShared::GP_LOGIN_BAD_UNIQUENICK);
					break;
				case TaskShared::WebErrorCode_BackendError:
					code = (GPShared::GP_NETWORK);
					break;
			}
			s << code;
			s << "\\ndone\\";
			((Peer*)peer)->SendPacket(s.str().c_str());
		}
	}
	void Peer::m_nicks_cb(TaskShared::WebErrorDetails error_details, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void* extra, INetPeer* peer) {
		std::ostringstream s;

		s << "\\nr\\" << results.size();

		std::vector<OS::Profile>::iterator it = results.begin();
		while (it != results.end()) {
			OS::Profile p = *it;
			s << "\\nick\\" << p.nick;
			s << "\\uniquenick\\" << p.uniquenick;
			it++;
		}

		s << "\\ndone\\";

		((Peer*)peer)->SendPacket(s.str().c_str());
	}
	void Peer::handle_nicks(OS::KVReader data_parser) {
		TaskShared::AuthRequest request;
		request.type = TaskShared::EAuthType_User_EmailPassword;

		if (data_parser.HasKey("userid")) {
			request.user.id = data_parser.GetValueInt("userid");
		}

		if (data_parser.HasKey("partnercode")) {
			request.user.partnercode = data_parser.GetValueInt("partnercode");
		}
		else if (data_parser.HasKey("partnerid")) {
			request.user.partnercode = data_parser.GetValueInt("partnerid");
		}

		if (data_parser.HasKey("email")) {
			request.user.email = data_parser.GetValue("email");
		}

		if (data_parser.HasKey("namespaceid")) {
			request.profile.namespaceid = data_parser.GetValueInt("namespaceid");
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

		request.user.password = password;
		request.extra = (void *)request.profile.namespaceid;
		request.peer = this;
		IncRef();
		request.callback = Peer::m_nicks_auth_cb;
		TaskScheduler<TaskShared::AuthRequest, TaskThreadData>* scheduler = ((SM::Server*)(GetDriver()->getServer()))->GetAuthTask();
		scheduler->AddRequest(request.type, request);
	}
}