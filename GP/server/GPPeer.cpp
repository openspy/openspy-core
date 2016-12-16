#include "GPPeer.h"
#include "GPDriver.h"
#include <OS/OpenSpy.h>
#include <OS/Search/Profile.h>
#include <OS/legacy/helpers.h>
#include <OS/legacy/buffreader.h>
#include <OS/legacy/buffwriter.h>
#include <OS/socketlib/socketlib.h>

#include <sstream>

namespace GP {
	GPErrorData Peer::m_error_data[] = {
		// General error codes.
		{GP_GENERAL, "There was an unknown error. ", true},
		{GP_PARSE, "Unexpected data was received from the server.", true},
		{GP_NOT_LOGGED_IN, "The request cannot be processed because user has not logged in. ", true},
		{GP_BAD_SESSKEY, "The request cannot be processed because of an invalid session key.", true},
		{GP_DATABASE, "There was a database error.", true},
		{GP_NETWORK, "There was an error connecting a network socket.", true},
		{GP_FORCED_DISCONNECT, "This profile has been disconnected by another login.", true},
		{GP_CONNECTION_CLOSED, "The server has closed the connection.", true},
		{GP_UDP_LAYER, "There was a problem with the UDP layer.", true},
		// Error codes that can occur while logging in.
		{GP_LOGIN, "There was an error logging in to the GP backend.", false},
		{GP_LOGIN_TIMEOUT, "The login attempt timed out.", false},
		{GP_LOGIN_BAD_NICK, "The nickname provided was incorrect.", false},
		{GP_LOGIN_BAD_EMAIL, "The e-mail address provided was incorrect.", false},
		{GP_LOGIN_BAD_PASSWORD, "The password provided was incorrect.", false},
		{GP_LOGIN_BAD_PROFILE, "The profile provided was incorrect.", false},
		{GP_LOGIN_PROFILE_DELETED, "The profile has been deleted.", false},
		{GP_LOGIN_CONNECTION_FAILED, "The server has refused the connection.", false},
		{GP_LOGIN_SERVER_AUTH_FAILED, "The server could not be authenticated.", false},
		{GP_LOGIN_BAD_UNIQUENICK, "The uniquenick provided was incorrect.", false},
		{GP_LOGIN_BAD_PREAUTH, "There was an error validating the pre-authentication.", false},
		{GP_LOGIN_BAD_LOGIN_TICKET, "The login ticket was unable to be validated.", false},
		{GP_LOGIN_EXPIRED_LOGIN_TICKET, "The login ticket had expired and could not be used.", false},

		// Error codes that can occur while creating a new user.
		{GP_NEWUSER, "There was an error creating a new user.", false},
		{GP_NEWUSER_BAD_NICK, "A profile with that nick already exists.", false},
		{GP_NEWUSER_BAD_PASSWORD, "The password does not match the email address.", false},
		{GP_NEWUSER_UNIQUENICK_INVALID, "The uniquenick is invalid.", false},
		{GP_NEWUSER_UNIQUENICK_INUSE, "The uniquenick is already in use.", false},

		// Error codes that can occur while updating user information.
		{GP_UPDATEUI, "There was an error updating the user information.", false},
		{GP_UPDATEUI_BAD_EMAIL, "A user with the email address provided already exists.", false},

		// Error codes that can occur while creating a new profile.
		{GP_NEWPROFILE, "There was an error creating a new profile.", false},
		{GP_NEWPROFILE_BAD_NICK, "The nickname to be replaced does not exist.", false},
		{GP_NEWPROFILE_BAD_OLD_NICK, "A profile with the nickname provided already exists.", false},

		// Error codes that can occur while updating profile information.
		{GP_UPDATEPRO, "There was an error updating the profile information. ", false},
		{GP_UPDATEPRO_BAD_NICK, "A user with the nickname provided already exists.", false},

		// Error codes that can occur while creating a new profile.
		{GP_NEWPROFILE,"There was an error creating a new profile.", false},
		{GP_NEWPROFILE_BAD_NICK,"The nickname to be replaced does not exist.", false},
		{GP_NEWPROFILE_BAD_OLD_NICK,"A profile with the nickname provided already exists.", false},

		// Error codes that can occur while updating profile information.
		{GP_UPDATEPRO,"There was an error updating the profile information. ", false},
		{GP_UPDATEPRO_BAD_NICK,"A user with the nickname provided already exists.", false},

		// Error codes that can occur while adding someone to your buddy list.
		{GP_ADDBUDDY,"There was an error adding a buddy. ", false},
		{GP_ADDBUDDY_BAD_FROM,"The profile requesting to add a buddy is invalid.", false},
		{GP_ADDBUDDY_BAD_NEW,"The profile requested is invalid.", false},
		{GP_ADDBUDDY_ALREADY_BUDDY,"The profile requested is already a buddy.", false},
		{GP_ADDBUDDY_IS_ON_BLOCKLIST,"The profile requested is on the local profile's block list.", false},
		//DOM-IGNORE-BEGIN 
		{GP_ADDBUDDY_IS_BLOCKING,"Reserved for future use.", false},
		//DOM-IGNORE-END

		// Error codes that can occur while being authorized to add someone to your buddy list.
		{GP_AUTHADD,"There was an error authorizing an add buddy request.", false},
		{GP_AUTHADD_BAD_FROM,"The profile being authorized is invalid. ", false},
		{GP_AUTHADD_BAD_SIG,"The signature for the authorization is invalid.", false},
		{GP_AUTHADD_IS_ON_BLOCKLIST,"The profile requesting authorization is on a block list.", false},
		//DOM-IGNORE-BEGIN 
		{GP_AUTHADD_IS_BLOCKING,"Reserved for future use.", false},
		//DOM-IGNORE-END

		// Error codes that can occur with status messages.
		{GP_STATUS,"There was an error with the status string.", false},

		// Error codes that can occur while sending a buddy message.
		{GP_BM,"There was an error sending a buddy message.", false},
		{GP_BM_NOT_BUDDY,"The profile the message was to be sent to is not a buddy.", false},
		{GP_BM_EXT_INFO_NOT_SUPPORTED,"The profile does not support extended info keys.", false},
		{GP_BM_BUDDY_OFFLINE,"The buddy to send a message to is offline.", false},

		// Error codes that can occur while getting profile information.
		{GP_GETPROFILE,"There was an error getting profile info. ", false},
		{GP_GETPROFILE_BAD_PROFILE,"The profile info was requested on is invalid.", false},

		// Error codes that can occur while deleting a buddy.
		{GP_DELBUDDY,"There was an error deleting the buddy.", false},
		{GP_DELBUDDY_NOT_BUDDY,"The buddy to be deleted is not a buddy. ", false},

		// Error codes that can occur while deleting your profile.
		{GP_DELPROFILE,"There was an error deleting the profile.", false},
		{GP_DELPROFILE_LAST_PROFILE,"The last profile cannot be deleted.", false},

		// Error codes that can occur while searching for a profile.
		{GP_SEARCH,"There was an error searching for a profile.", false},
		{GP_SEARCH_CONNECTION_FAILED,"The search attempt failed to connect to the server.", false},
		{GP_SEARCH_TIMED_OUT,"The search did not return in a timely fashion.", false},

		// Error codes that can occur while checking whether a user exists.
		{GP_CHECK,"There was an error checking the user account.", false},
		{GP_CHECK_BAD_EMAIL,"No account exists with the provided e-mail address.", false},
		{GP_CHECK_BAD_NICK,"No such profile exists for the provided e-mail address.", false},
		{GP_CHECK_BAD_PASSWORD,"The password is incorrect.", false},

		// Error codes that can occur while revoking buddy status.
		{GP_REVOKE,"There was an error revoking the buddy.", false},
		{GP_REVOKE_NOT_BUDDY,"You are not a buddy of the profile.", false},

		// Error codes that can occur while registering a new unique nick.
		{GP_REGISTERUNIQUENICK, "There was an error registering the uniquenick.", false},
		{GP_REGISTERUNIQUENICK_TAKEN, "The uniquenick is already taken.", false},
		{GP_REGISTERUNIQUENICK_RESERVED, "The uniquenick is reserved. ", false},
		{GP_REGISTERUNIQUENICK_BAD_NAMESPACE, "Tried to register a nick with no namespace set. ", false},

		// Error codes that can occur while registering a CD key.
		{GP_REGISTERCDKEY, "There was an error registering the cdkey.", false},
		{GP_REGISTERCDKEY_BAD_KEY, "The cdkey is invalid. ", false},
		{GP_REGISTERCDKEY_ALREADY_SET, "The profile has already been registered with a different cdkey.", false},
		{GP_REGISTERCDKEY_ALREADY_TAKEN, "The cdkey has already been registered to another profile. ", false},

		// Error codes that can occur while adding someone to your block list.
		{GP_ADDBLOCK, "There was an error adding the player to the blocked list. ", false},
		{GP_ADDBLOCK_ALREADY_BLOCKED, "The profile specified is already blocked.", false},

		// Error codes that can occur while removing someone from your block list.
		{GP_REMOVEBLOCK, "There was an error removing the player from the blocked list. ", false},
		{GP_REMOVEBLOCK_NOT_BLOCKED, "The profile specified was not a member of the blocked list.", false}

	};

	Peer::Peer(Driver *driver, struct sockaddr_in *address_info, int sd) {
		m_sd = sd;
		mp_driver = driver;
		m_address_info = *address_info;
		m_delete_flag = false;
		m_timeout_flag = false;
		mp_backend_session_key = NULL;
		gettimeofday(&m_last_ping, NULL);
		printf("GP New peer\n");

		m_status.status = GP_OFFLINE;

		strcpy(m_challenge,"1234567890");

		OS::Profile p;
		p.id = 100666;
		p.nick = "SomeGuy";
		p.uniquenick = "SomeGuyUnique";
		p.deleted = 0;
		p.namespaceid = 1;
		m_buddies.push_back(p);

		p.id = 100667;
		p.nick = "asdfasd";
		p.uniquenick = "Sasdasd";
		p.deleted = 0;
		p.namespaceid = 1;
		m_buddies.push_back(p);

		send_login_challenge(1);
	}
	Peer::~Peer() {
		if(mp_backend_session_key) {
			free((void *)mp_backend_session_key);
		}
		close(m_sd);
		printf("Peer delete\n");
	}
	void Peer::think(bool packet_waiting) {
		char buf[GPI_READ_SIZE + 1];
		socklen_t slen = sizeof(struct sockaddr_in);
		int len, piece_len;
		if (packet_waiting) {
			len = recv(m_sd, (char *)&buf, GPI_READ_SIZE, 0);
			buf[len] = 0;
			if(len == 0) goto end;

			/* split by \\final\\  */
			char *p = (char *)&buf;
			char *x;
			while(true) {
				x = p;
				p = strstr(p,"\\final\\");
				if(p == NULL) { break; }
				*p = 0;
				piece_len = strlen(x);
				this->handle_packet(x, piece_len);
				p+=7; //skip final
			}

			piece_len = strlen(x);
			if(piece_len > 0) {
				this->handle_packet(x, piece_len);
			}
			
		}

		end:
		send_ping();

		//check for timeout
		struct timeval current_time;
		gettimeofday(&current_time, NULL);
		if(current_time.tv_sec - m_last_recv.tv_sec > GP_PING_TIME*2) {
			m_delete_flag = true;
			m_timeout_flag = true;
			printf("D/C from timeout\n");
		}
	}
	void Peer::handle_packet(char *data, int len) {
		printf("GP Handle(%d): %s\n", len,data);
		char command[32];
		if(!find_param(0, data,(char *)&command, sizeof(command))) {
			m_delete_flag = true;
			return;
		}
		gettimeofday(&m_last_recv, NULL);
		if(!strcmp(command, "login")) {
			handle_login(data, len);
			return;
		} else if(strcmp(command, "ka")) {
			handle_keepalive(data, len);
		}
		if(mp_backend_session_key) {
			if(!strcmp(command, "status")) {
				handle_status(data, len);
			} else if(!strcmp(command, "addbuddy")) {
				handle_addbuddy(data, len);
			} else if(!strcmp(command, "delbuddy")) {
				handle_addbuddy(data, len);
			} else if(!strcmp(command, "authadd")) {
				handle_authadd(data, len);
			} else if(!strcmp(command, "getprofile")) {
				handle_getprofile(data, len);
			}
		}
		printf("Got cmd: %s\n", command);
		
	}
	void Peer::handle_login(const char *data, int len) {
		char challenge[128 + 1];
		char user[84 + 1];
		char gamename[33 + 1];
		char response[33 + 1];
		int partnercode = find_paramint("partnerid",(char *)data);
		int peer_port = find_paramint("port",(char *)data);
		int sdkrev = find_paramint("sdkrevision",(char *)data);
		int namespaceid = find_paramint("namespaceid",(char *)data);
		int quiet = find_paramint("quiet",(char *)data);

		m_auth_operation_id = find_paramint("id",(char *)data);		
		int type = 0;

		if(!find_param("challenge",(char *)data, (char *)&challenge,sizeof(challenge)-1)) {
			m_delete_flag = true;
			return;
		}
		if(!find_param("user",(char *)data, (char *)&user,sizeof(user)-1)) {
			m_delete_flag = true;
			return;
		} else {
			type = 1;
		}
		if(!find_param("response",(char *)data, (char *)&response,sizeof(response)-1)) {
			m_delete_flag = true;
			return;
		}
		printf("Login got (%s) - (%s) - (%s)\n",challenge,user,response);

		if(type == 1) {
			perform_nick_email_auth(user, partnercode, m_challenge, challenge, response);
		}
	}
	void Peer::handle_status(const char *data, int len) {
		m_status.status = (GPEnum)find_paramint("status",(char *)data);

		find_param("statstring",(char *)data, (char *)&m_status.status_str,GP_STATUS_STRING_LEN);
		find_param("locstring",(char *)data, (char *)&m_status.location_str,GP_LOCATION_STRING_LEN);
	}
	void Peer::handle_statusinfo(const char *data, int len) {

	}
	void Peer::handle_addbuddy(const char *data, int len) {
		int newprofileid = find_paramint("newprofileid",(char *)data);
		char reason[GP_REASON_LEN + 1];
		find_param("reason",(char *)data, (char *)&reason,GP_REASON_LEN);

		//CS::TryAddBuddy(m_profile.id, newprofileid, reason);
	}
	void Peer::send_add_buddy_request(int from_profileid, const char *reason) {
		////\bm\1\f\157928340\msg\I have authorized your request to add me to your list\final
		std::ostringstream s;
		s << "\\bm\\" << GPI_BM_REQUEST;
		s << "\\f\\" << from_profileid;
		s << "\\msg\\" << reason;
		s << "|signed|d41d8cd98f00b204e9800998ecf8427e"; //temp until calculation fixed
		SendPacket((const uint8_t *)s.str().c_str(),s.str().length());
	}
	void Peer::handle_delbuddy(const char *data, int len) {
		int delprofileid = find_paramint("delprofileid",(char *)data);
	}
	void Peer::handle_authadd(const char *data, int len) {
		int fromprofileid = find_paramint("fromprofileid",(char *)data);
		//CS::AuthorizeBuddy(m_profile.id, fromprofileid);
	}
	void Peer::handle_getprofile(const char *data, int len) {
		OS::ProfileSearchRequest request;
		int profileid = find_paramint("profileid",(char *)data);		
		m_search_operation_id = find_paramint("id",(char *)data);		
		request.profileid = profileid;
		request.extra = this;
		request.callback = Peer::m_getprofile_callback;
		OS::ProfileSearchTask::getProfileTask()->AddRequest(request);
	}
	void Peer::handle_keepalive(const char *data, int len) {
		std::ostringstream s;
		s << "\\ka\\";
		SendPacket((const uint8_t *)s.str().c_str(),s.str().length());
	}
	void Peer::m_getprofile_callback(bool success, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra) {
		Peer *peer = (Peer *)extra;
		if(!g_gbl_gp_driver->HasPeer(peer)) {
			return;
		}
		std::vector<OS::Profile>::iterator it = results.begin();
		while(it != results.end()) {
			OS::Profile p = *it;
			OS::User user = result_users[p.userid];
			std::ostringstream s;

			s << "\\pi\\";
			s << "\\profileid\\" << p.id;

			if(p.nick.length()) {
				s << "\\nick\\" << p.nick;
			}

			if(p.uniquenick.length()) {
				s << "\\uniquenick\\" << p.uniquenick;
			}

			if(user.email.length()) {
				s << "\\email\\" << user.email;
			}

			if(p.firstname.length()) {
				s << "\\firstname\\" << p.firstname;
			}

			if(p.lastname.length()) {
				s << "\\lastname\\" << p.lastname;
			}

			if(p.icquin) {
				s << "\\icquin\\" << p.icquin;
			}

			if(p.zipcode) {
				s << "\\zipcode\\" << p.zipcode;
			}

			s << "\\sex\\" << p.sex;

			s << "\\id\\" << peer->m_search_operation_id;

			s << "\\sig\\d41d8cd98f00b204e9800998ecf8427e"; //temp until calculation fixed
			peer->SendPacket((const uint8_t *)s.str().c_str(),s.str().length());
			it++;
		}
	}
	void Peer::send_login_challenge(int type) {
		std::ostringstream s;

		s << "\\lc\\" << type;
		switch(type) {
			case 1: {
				s << "\\challenge\\" << m_challenge;
				s << "\\id\\" << GPI_CONNECTING;
				break;
			}
		}
		SendPacket((const uint8_t *)s.str().c_str(),s.str().length());
	}
	void Peer::SendPacket(const uint8_t *buff, int len, bool attach_final) {
		uint8_t out_buff[GPI_READ_SIZE + 1];
		uint8_t *p = (uint8_t*)&out_buff;
		int out_len = 0;
		BufferWriteData(&p, &out_len, buff, len);
		if(attach_final) {
			BufferWriteData(&p, &out_len, (uint8_t*)"\\final\\", 7);
		}
		int c = send(m_sd, (const char *)&out_buff, out_len, MSG_NOSIGNAL);
		if(c < 0) {
			m_delete_flag = true;
		}
	}
	void Peer::send_ping() {
		//check for timeout
		struct timeval current_time;

		gettimeofday(&current_time, NULL);
		if(current_time.tv_sec - m_last_ping.tv_sec > GP_PING_TIME) {
			std::string ping_packet = "\\ka\\";
			SendPacket((const uint8_t *)ping_packet.c_str(),ping_packet.length());
		}
	}
	void Peer::perform_nick_email_auth(const char *nick_email, int partnercode, const char *server_challenge, const char *client_challenge, const char *response) {
		const char *email = NULL;
		char nick[31 + 1];
		const char *first_at = strchr(nick_email, '@');
		if(first_at) {
			int nick_len = first_at - nick_email;
			if(nick_len < sizeof(nick)) {
				strncpy(nick, nick_email, nick_len);
				nick[nick_len] = 0;
			}
			email = first_at+1;
		}
 		OS::AuthTask::TryAuthNickEmail_GPHash(nick, email, partnercode, server_challenge, client_challenge, response, m_nick_email_auth_cb, this);

	}
	void Peer::m_nick_email_auth_cb(bool success, OS::User user, OS::Profile profile, OS::AuthData auth_data, void *extra) {
		Peer *peer = (Peer *)extra;
		if(!g_gbl_gp_driver->HasPeer(peer)) {
			return;
		}
		if(!peer->mp_backend_session_key && auth_data.session_key)
			peer->mp_backend_session_key = strdup(auth_data.session_key);

		peer->m_user = user;
		peer->m_profile = profile;

		std::ostringstream ss;
		if(success) {
			ss << "\\lc\\2";

			ss << "\\sesskey\\1";

			ss << "\\userid\\" << user.id;

			ss << "\\profileid\\" << profile.id;

			if(profile.uniquenick.length()) {
				ss << "\\uniquenick\\" << profile.uniquenick;
			}
			if(auth_data.session_key) {
				ss << "\\lt\\" << auth_data.session_key;
			}
			if(auth_data.hash_proof) {
				ss << "\\proof\\" << auth_data.hash_proof;
			}
			ss << "\\id\\" << peer->m_auth_operation_id;

			peer->SendPacket((const uint8_t *)ss.str().c_str(),ss.str().length());

			peer->send_buddies();
		} else {
			switch(auth_data.response_code) {
				case OS::LOGIN_RESPONSE_USER_NOT_FOUND:
					peer->send_error(GP_LOGIN_BAD_EMAIL);
				break;
				case OS::LOGIN_RESPONSE_INVALID_PASSWORD:
					peer->send_error(GP_LOGIN_BAD_PASSWORD);
				break;
				case OS::LOGIN_RESPONSE_INVALID_PROFILE:
					peer->send_error(GP_LOGIN_BAD_PROFILE);
				break;
				case OS::LOGIN_RESPONSE_UNIQUE_NICK_EXPIRED:
					peer->send_error(GP_LOGIN_BAD_UNIQUENICK);
				break;
				case OS::LOGIN_RESPONSE_DB_ERROR:
					peer->send_error(GP_DATABASE);
				break;
				case OS::LOGIN_RESPONSE_SERVERINITFAILED:
				case OS::LOGIN_RESPONSE_SERVER_ERROR:
					peer->send_error(GP_NETWORK);
				break;
			}
		}
	}
	void Peer::send_buddies() {
		std::vector<OS::Profile>::iterator it = m_buddies.begin();
		std::ostringstream ss;
		ss << "\\bdy\\" << m_buddies.size();
		ss << "\\list\\";
		while(it != m_buddies.end()) {
			OS::Profile p = *it;
			ss << p.id << ",";
			it++;
		}
		std::string str = ss.str();
		str = str.substr(0, str.size()-1);
		SendPacket((const uint8_t *)str.c_str(),str.length());
	}
	void Peer::send_error(GPErrorCode code) {
		for(int i=0;i<sizeof(Peer::m_error_data)/sizeof(GPErrorData);i++) {
			if(Peer::m_error_data[i].error == code) {
				std::ostringstream ss;
				ss << "\\error\\";
				ss << "\\err\\" << Peer::m_error_data[i].error;
				ss << "\\errmsg\\" << Peer::m_error_data[i].msg;
				if(Peer::m_error_data[i].die)
					ss << "\\fatal\\" << Peer::m_error_data[i].die;
				SendPacket((const uint8_t *)ss.str().c_str(),ss.str().length());
			}
		}
	}
}