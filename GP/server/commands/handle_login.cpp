#include <OS/OpenSpy.h>
#include <OS/Search/Profile.h>

#include <OS/Search/User.h>

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
    	void Peer::handle_login(OS::KVReader data_parser) {
		int partnercode = data_parser.GetValueInt("partnerid");
		//int peer_port = data_parser.GetValueInt("port");
		//int sdkrev = data_parser.GetValueInt("sdkrevision");
		int namespaceid = data_parser.GetValueInt("namespaceid");
		//int quiet = data_parser.GetValueInt("quiet");

		int operation_id = data_parser.GetValueInt("id");
		int type = 0;

		if (!data_parser.HasKey("challenge")) {
			send_error(GPShared::GP_PARSE);
			return;
		}
		if (data_parser.HasKey("user")) {
			type = 1;
		}
		else if (data_parser.HasKey("authtoken")) {
			type = 2;
		}
		else if (data_parser.HasKey("uniquenick")) {
			type = 3;
		}
		else {
			send_error(GPShared::GP_LOGIN_CONNECTION_FAILED);
			return;
		}
		if (!data_parser.HasKey("response")) {
			send_error(GPShared::GP_PARSE);
			return;
		}
		std::string challenge = data_parser.GetValue("challenge");
		std::string user = data_parser.GetValue("user");
		std::string response = data_parser.GetValue("response");

		if(type == 1) {
			perform_nick_email_auth(user.c_str(), partnercode, namespaceid, m_challenge, challenge.c_str(), response.c_str(), operation_id, this);
		}
		else if (type == 2) {
			std::string authtoken = data_parser.GetValue("authtoken");
			perform_preauth_auth(authtoken.c_str(), m_challenge, challenge.c_str(), response.c_str(), operation_id, this);
		}
		else if (type == 3) {
			std::string uniquenick = data_parser.GetValue("uniquenick");
			perform_uniquenick_auth(uniquenick.c_str(), partnercode, namespaceid, m_challenge, challenge.c_str(), response.c_str(), operation_id, this);
		}
	}
	void Peer::perform_uniquenick_auth(const char *uniquenick, int partnercode, int namespaceid, const char *server_challenge, const char *client_challenge, const char *response, int operation_id, INetPeer *peer) {
		TaskScheduler<GP::GPBackendRedisRequest, TaskThreadData> *scheduler = ((GP::Server *)(GetDriver()->getServer()))->GetGPTask();
		GPBackendRedisRequest req;
		req.type = EGPRedisRequestType_Auth_Uniquenick_GPHash;
		req.profile.uniquenick = uniquenick;
		req.profile.namespaceid = namespaceid;
		req.server_challenge = server_challenge;
		req.client_challenge = client_challenge;
		req.client_response = response;
		req.authCallback = m_auth_cb;
		req.extra = (void *)operation_id;
		req.user.partnercode = partnercode;
		req.peer = this;
		req.peer->IncRef();

		scheduler->AddRequest(req.type, req);
	}
	void Peer::perform_nick_email_auth(const char *nick_email, int partnercode, int namespaceid, const char *server_challenge, const char *client_challenge, const char *response, int operation_id, INetPeer *peer) {
		const char *email = NULL;
		char nick[31 + 1];
		const char *first_at = strchr(nick_email, '@');
		if(first_at) {
			unsigned int nick_len = (unsigned int)(first_at - nick_email);
			if(nick_len < sizeof(nick)) {
				strncpy(nick, nick_email, nick_len);
				nick[nick_len] = 0;
			}
			email = first_at+1;
		}

		TaskScheduler<GP::GPBackendRedisRequest, TaskThreadData> *scheduler = ((GP::Server *)(GetDriver()->getServer()))->GetGPTask();
		GPBackendRedisRequest req;
		req.type = EGPRedisRequestType_Auth_NickEmail_GPHash;
		req.profile.nick = nick;
		req.profile.namespaceid = namespaceid;
		req.user.email = email;
		req.server_challenge = server_challenge;
		req.client_challenge = client_challenge;
		req.authCallback = m_auth_cb;
		req.client_response = response;
		req.extra = (void *)operation_id;
		req.user.partnercode = partnercode;
		req.peer = this;
		req.peer->IncRef();
		
		scheduler->AddRequest(req.type, req);
	}
	void Peer::perform_preauth_auth(const char *auth_token, const char *server_challenge, const char *client_challenge, const char *response, int operation_id, INetPeer *peer) {
		TaskScheduler<GP::GPBackendRedisRequest, TaskThreadData> *scheduler = ((GP::Server *)(GetDriver()->getServer()))->GetGPTask();
		GPBackendRedisRequest req;
		req.type = EGPRedisRequestType_Auth_PreAuth_Token_GPHash;
		req.server_challenge = server_challenge;
		req.client_challenge = client_challenge;
		req.authCallback = m_auth_cb;
		req.client_response = response;
		req.extra = (void *)operation_id;
		req.auth_token = auth_token;
		req.peer = this;
		req.peer->IncRef();

		scheduler->AddRequest(req.type, req);
	}
	void Peer::m_auth_cb(bool success, OS::User user, OS::Profile profile, TaskShared::AuthData auth_data, void *extra, INetPeer *peer) {
		if(!((GP::Peer *)peer)->m_backend_session_key.length() && auth_data.session_key.length())
			((GP::Peer *)peer)->m_backend_session_key = auth_data.session_key;

		((GP::Peer *)peer)->m_user = user;
		((GP::Peer *)peer)->m_profile = profile;

		int operation_id = (int)extra;

		std::ostringstream ss;
		if(success) {
			ss << "\\lc\\2";

			ss << "\\sesskey\\1";

			ss << "\\userid\\" << user.id;

			ss << "\\profileid\\" << profile.id;
			
			if(profile.uniquenick.length()) {
				ss << "\\uniquenick\\" << profile.uniquenick;
			}
			if(auth_data.session_key.length()) {
				ss << "\\lt\\" << auth_data.session_key;
			}
			if(auth_data.response_proof.length()) {
				ss << "\\proof\\" << auth_data.response_proof;
			}
			ss << "\\id\\" << operation_id;

			((GP::Peer *)peer)->SendPacket((const uint8_t *)ss.str().c_str(),ss.str().length());

			((GP::Peer *)peer)->send_buddies();
			((GP::Peer *)peer)->send_blocks();
		} else {
			switch(auth_data.error_details.response_code) {
				case TaskShared::WebErrorCode_NoSuchUser:
					((GP::Peer *)peer)->send_error(GP_LOGIN_BAD_EMAIL);
				break;
				case TaskShared::WebErrorCode_AuthInvalidCredentials:
					((GP::Peer *)peer)->send_error(GP_LOGIN_BAD_PASSWORD);
				break;
				case TaskShared::WebErrorCode_NickInvalid:
					((GP::Peer *)peer)->send_error(GP_LOGIN_BAD_PROFILE);
				break;
				case TaskShared::WebErrorCode_UniqueNickInvalid:
					((GP::Peer *)peer)->send_error(GP_LOGIN_BAD_UNIQUENICK);
				break;
				case TaskShared::WebErrorCode_BackendError:
					((GP::Peer *)peer)->send_error(GP_NETWORK);
				break;
			}
		}
	}
}