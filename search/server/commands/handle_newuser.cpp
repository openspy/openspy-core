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
	void Peer::m_newuser_cb(bool success, OS::User user, OS::Profile profile, TaskShared::UserRegisterData auth_data, void *extra, INetPeer *peer) {
		int err_code = 0;

		if (auth_data.error_details.response_code != TaskShared::WebErrorCode_Success) {
			profile.id = auth_data.error_details.profileid;
		}
		switch(auth_data.error_details.response_code) {
			case TaskShared::WebErrorCode_UniqueNickInUse:
				err_code = GPShared::GP_NEWUSER_UNIQUENICK_INUSE;
				break;
			case TaskShared::WebErrorCode_UserExists:
				err_code = GPShared::GP_NEWUSER_BAD_PASSWORD;
				break;
			case TaskShared::WebErrorCode_NickInvalid:
				err_code = GPShared::GP_NEWUSER_BAD_NICK;
			break;
			case TaskShared::WebErrorCode_UniqueNickInvalid:
				err_code = GPShared::GP_NEWUSER_UNIQUENICK_INVALID;
			break;
			case TaskShared::WebErrorCode_Success:
			break;
			default:
				err_code = GPShared::GP_NEWUSER;
			break;
		}
		std::ostringstream s;
		s << "\\nur\\" << err_code;
		s << "\\pid\\" << profile.id;

		((Peer *)peer)->m_profile = profile;

		//((Peer *)peer)->post_register_registercdkey(); //disabled until productids are mapped to gameids

		((Peer *)peer)->SendPacket(s.str().c_str());

		((Peer *)peer)->Delete();
	}
	void Peer::handle_newuser(OS::KVReader data_parser) {
		std::string nick;
		std::string uniquenick;
		std::string email;

		if (!data_parser.HasKey("email") || !data_parser.HasKey("nick")) {
			send_error(GPShared::GP_PARSE);
			return;
		}

		email = data_parser.GetValue("email");
		nick = data_parser.GetValue("nick");


		if (data_parser.HasKey("uniquenick")) {
			uniquenick = data_parser.GetValue("uniquenick");
		}

		if (data_parser.HasKey("cdkey")) {
			m_postregister_cdkey = data_parser.GetValue("cdkey");
		}
		else if (data_parser.HasKey("cdkeyenc")) {
			std::string cdkeyenc = data_parser.GetValue("cdkeyenc");
			int passlen = (int)cdkeyenc.length();
			char *dpass = (char *)base64_decode((uint8_t *)cdkeyenc.c_str(), &passlen);
			passlen = gspassenc((uint8_t *)dpass);
			m_postregister_cdkey = dpass;
			if (dpass)
				free((void *)dpass);
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

		TaskShared::UserRequest req;
		req.type = TaskShared::EUserRequestType_Create;
		req.peer = this;
		req.peer->IncRef();

		req.profile_params.nick = nick;
		req.profile_params.uniquenick = uniquenick;
		if (data_parser.HasKey("namespaceid")) {
			req.profile_params.namespaceid = data_parser.GetValueInt("namespaceid");;
		}
		
		req.search_params.email = email;
		if (data_parser.HasKey("partnerid")) {
			req.search_params.partnercode = data_parser.GetValueInt("partnerid");
		}
		
		req.search_params.password = password;
		req.registerCallback = m_newuser_cb;

		TaskScheduler<TaskShared::UserRequest, TaskThreadData> *scheduler = ((SM::Server *)(GetDriver()->getServer()))->GetUserTask();
		scheduler->AddRequest(req.type, req);

	}
	void Peer::post_register_registercdkey() {
		TaskShared::CDKeyRequest request;

		request.gameid = -1;
		request.cdkey = m_postregister_cdkey;
		request.profile = m_profile;
		request.extra = NULL;
		request.peer = this;
		request.peer->IncRef();
		request.type = TaskShared::ECDKeyType_AssociateToProfile;
		request.callback = NULL;
		TaskScheduler<TaskShared::CDKeyRequest, TaskThreadData> *scheduler = ((SM::Server *)(GetDriver()->getServer()))->GetCDKeyTasks();
		scheduler->AddRequest(request.type, request);
	}
}