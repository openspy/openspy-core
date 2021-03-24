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
	void Peer::handle_newuser(OS::KVReader data_parser) {
		std::string nick;
		std::string uniquenick;
		std::string email;
		std::string passenc;
		std::string password;
		std::string gamename;
		//int partnercode = data_parser.GetValueInt("partnerid");
		int partnercode = 0;
		if (data_parser.HasKey("partnerid")) {
			partnercode = data_parser.GetValueInt("partnerid");
		}
		else if (data_parser.HasKey("partnercode")) {
			partnercode = data_parser.GetValueInt("partnercode");
		}
		int namespaceid = data_parser.GetValueInt("namespaceid");
		if (data_parser.HasKey("email")) {
			email = data_parser.GetValue("email");
		}
		else {
			send_error(GP_PARSE);
			return;
		}
		if (data_parser.HasKey("nick")) {
			nick = data_parser.GetValue("nick");
		}
		if (data_parser.HasKey("uniquenick")) {
			uniquenick = data_parser.GetValue("uniquenick");
		}
		if (data_parser.HasKey("gamename")) {
			gamename = data_parser.GetValue("gamename");
		}

		if (data_parser.HasKey("passenc")) {
			passenc = data_parser.GetValue("passenc");
			int passlen = (int)passenc.length();
			char *dpass = (char *)base64_decode((uint8_t *)passenc.c_str(), &passlen);
			passlen = gspassenc((uint8_t *)dpass);
			password = dpass;
			if (dpass)
				free((void *)dpass);
		}
		else if (data_parser.HasKey("passwordenc")) {
			passenc = data_parser.GetValue("passwordenc");
			int passlen = (int)passenc.length();
			char *dpass = (char *)base64_decode((uint8_t *)passenc.c_str(), &passlen);
			passlen = gspassenc((uint8_t *)dpass);
			password = dpass;
			if (dpass)
				free((void *)dpass);
		}
		else if (data_parser.HasKey("password")) {
			password = data_parser.GetValue("password");
		}
		else {
			send_error(GP_PARSE);
			return;
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

		int id = data_parser.GetValueInt("id");

		TaskShared::UserRequest req;
		req.type = TaskShared::EUserRequestType_Create;
		req.peer = this;
		req.extra = (void *)id;
		req.peer->IncRef();

		req.profile_params.nick = nick;
		req.profile_params.uniquenick = uniquenick;
		req.profile_params.namespaceid = namespaceid;
		req.search_params.email = email;
		req.search_params.password = password;
		req.search_params.partnercode = partnercode;
		req.registerCallback = m_newuser_cb;

		req.gamename = gamename;

		TaskScheduler<TaskShared::UserRequest, TaskThreadData> *scheduler = ((GP::Server *)(GetDriver()->getServer()))->GetUserTask();
		scheduler->AddRequest(req.type, req);
	}
	void Peer::m_newuser_cb(bool success, OS::User user, OS::Profile profile, TaskShared::UserRegisterData auth_data, void *extra, INetPeer *peer) {
		int err_code = 0;
		int operation_id = (int)extra;
		std::ostringstream s;

		//disabled due to session log-out conflict when games \login\ after \newuser\ 
		//((Peer *)peer)->m_user = user;
		//((Peer *)peer)->m_profile = profile;

		if (!success) {
			err_code = (int)GP_NEWUSER_BAD_NICK;
			switch (auth_data.error_details.response_code) {
			case TaskShared::WebErrorCode_UniqueNickInUse:
					err_code = GP_NEWUSER_UNIQUENICK_INUSE;
					break;
				case TaskShared::WebErrorCode_AuthInvalidCredentials:
					err_code = GP_NEWUSER_BAD_PASSWORD;
					break;
				case TaskShared::WebErrorCode_NickInvalid:
					err_code = GP_NEWUSER_BAD_NICK;
					break;
				case TaskShared::WebErrorCode_UniqueNickInvalid:
					err_code = GP_NEWUSER_UNIQUENICK_INVALID;
					break;
				default:
				case TaskShared::WebErrorCode_BackendError:
					err_code = GP_DATABASE;
			}
			if (profile.id != 0)
				s << "\\pid\\" << profile.id;
			s << "\\id\\" << operation_id;
			((Peer *)peer)->send_error((GPShared::GPErrorCode) err_code, s.str());
			return;
		}

		s << "\\nur\\";
		s << "\\userid\\" << user.id;
		s << "\\profileid\\" << profile.id;
		s << "\\id\\" << operation_id;
		
		((Peer *)peer)->SendPacket((const uint8_t*)s.str().c_str(), s.str().length());

		if (auth_data.gamedata.secretkey[0] != 0) {
			((Peer *)peer)->m_game = auth_data.gamedata;
		}
		else if (((Peer *)peer)->m_postregister_cdkey.length() > 0) {
			((Peer *)peer)->send_error(GPShared::GP_GENERAL, "Cannot register CD Key - Invalid game supplied");
			return;
		}

		//((Peer *)peer)->refresh_session();
	}

	void Peer::post_register_registercdkey() {
		TaskShared::CDKeyRequest request;
		
		request.gameid = m_game.gameid;
		request.cdkey = m_postregister_cdkey;
		request.profile = m_profile;
		request.extra = NULL;
		request.peer = this;
		request.peer->IncRef();
		request.type = TaskShared::ECDKeyType_AssociateToProfile;
		request.callback = NULL;
		TaskScheduler<TaskShared::CDKeyRequest, TaskThreadData> *scheduler = ((GP::Server *)(GetDriver()->getServer()))->GetCDKeyTasks();
		scheduler->AddRequest(request.type, request);
	}
}