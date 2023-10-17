#include <OS/OpenSpy.h>
#include <OS/Buffer.h>

#include <OS/gamespy/gamespy.h>
#include <OS/gamespy/gsmsalg.h>

#include <sstream>

#include <OS/GPShared.h>

#include <server/SMPeer.h>
#include <server/SMDriver.h>
#include <server/SMServer.h>

namespace SM {
	void Peer::m_nick_email_auth_cb(bool success, OS::User user, OS::Profile profile, TaskShared::AuthData auth_data, void *extra, INetPeer *peer) {
		std::ostringstream s;
		if(success) {
				s << "\\cur\\" << 0;
				s << "\\pid\\" << profile.id;
		} else {
				GPShared::GPErrorCode code = GPShared::GP_NETWORK;
				s << "\\cur\\";
				switch (auth_data.error_details.response_code) {
					case TaskShared::WebErrorCode_NoSuchUser:
							code = (GPShared::GP_CHECK_BAD_EMAIL);
							break;
					case TaskShared::WebErrorCode_AuthInvalidCredentials:
							code = (GPShared::GP_CHECK_BAD_PASSWORD);
							break;
					case TaskShared::WebErrorCode_NickInvalid:
							code = (GPShared::GP_CHECK_BAD_NICK);
							break;
					case TaskShared::WebErrorCode_UniqueNickInvalid:
							code = (GPShared::GP_CHECK_BAD_NICK);
							break;
					default:
					case TaskShared::WebErrorCode_BackendError:
							code = (GPShared::GP_CHECK);
							break;
				}
				s << code;
		}


		((Peer *)peer)->SendPacket(s.str().c_str());

		((Peer *)peer)->Delete();
	}
	void Peer::handle_check(OS::KVReader data_parser) {

		std::string nick;
		std::string uniquenick;
		std::string email;
		int namespaceid = data_parser.GetValueInt("namespaceid");

		if (!data_parser.HasKey("email") || !data_parser.HasKey("nick")) {
			return;
		}

		email = data_parser.GetValue("email");
		nick = data_parser.GetValue("nick");


		if (data_parser.HasKey("uniquenick")) {
			uniquenick = data_parser.GetValue("uniquenick");
		}

		std::string password;

		if (data_parser.HasKey("passenc")) {
			password = data_parser.GetValue("passenc");
			int passlen = (int)password.length();
			char* dpass = (char*)base64_decode((uint8_t*)password.c_str(), &passlen);
			passlen = gspassenc((uint8_t*)dpass, passlen);
			password = dpass;
			if (dpass)
				free((void*)dpass);
		} else if (data_parser.HasKey("passwordenc")) {
			password = data_parser.GetValue("passwordenc");
			int passlen = (int)password.length();
			char* dpass = (char*)base64_decode((uint8_t*)password.c_str(), &passlen);
			passlen = gspassenc((uint8_t*)dpass, passlen);
			password = dpass;
			if (dpass)
				free((void*)dpass);
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

		TaskShared::AuthRequest request;
		request.type = TaskShared::EAuthType_NickEmail;
		request.profile.nick = nick;

		if(data_parser.HasKey("namespaceid"))
			request.profile.namespaceid = namespaceid;

		request.user.email = email;
		request.user.password = password;

		if (data_parser.HasKey("partnerid"))
			request.user.partnercode = data_parser.GetValueInt("partnerid");
		request.extra = this;
		request.peer = this;
		request.create_session = false;
		request.callback = m_nick_email_auth_cb;
		IncRef();
		AddAuthTaskRequest(request);
	}

}