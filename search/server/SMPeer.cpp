#include "SMPeer.h"
#include "SMDriver.h"
#include <OS/OpenSpy.h>
#include <OS/Search/Profile.h>
#include <OS/legacy/helpers.h>
#include <OS/legacy/buffreader.h>
#include <OS/legacy/buffwriter.h>
#include <OS/socketlib/socketlib.h>
#include <OS/Search/User.h>
#include <OS/Search/Profile.h>
#include <OS/Auth.h>

#include <sstream>

enum { //TODO: move into shared header
	GP_NEWUSER						= 512, // 0x200, There was an error creating a new user.
	GP_NEWUSER_BAD_NICK				= 513, // 0x201, A profile with that nick already exists.
	GP_NEWUSER_BAD_PASSWORD			= 514, // 0x202, The password does not match the email address.
	GP_NEWUSER_UNIQUENICK_INVALID	= 515, // 0x203, The uniquenick is invalid.
	GP_NEWUSER_UNIQUENICK_INUSE		= 516, // 0x204, The uniquenick is already in use.
};

namespace SM {

	Peer::Peer(Driver *driver, struct sockaddr_in *address_info, int sd) {
		m_sd = sd;
		mp_driver = driver;
		m_address_info = *address_info;
		m_delete_flag = false;
		m_timeout_flag = false;
		gettimeofday(&m_last_ping, NULL);
	}
	Peer::~Peer() {
		close(m_sd);
	}
	void Peer::think(bool packet_waiting) {
		char buf[GPI_READ_SIZE + 1];
		socklen_t slen = sizeof(struct sockaddr_in);
		int len, piece_len;
		if (packet_waiting) {
			len = recv(m_sd, (char *)&buf, GPI_READ_SIZE, 0);
			buf[len] = 0;
			//if(len == 0) goto end;
			if(len == 0) return;

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

		//end:
		//check for timeout
		struct timeval current_time;
		gettimeofday(&current_time, NULL);
		if(current_time.tv_sec - m_last_recv.tv_sec > SP_PING_TIME*2) {
			m_delete_flag = true;
			m_timeout_flag = true;
		} else if(len == 0 && packet_waiting) {
			m_delete_flag = true;
		}
	}
	void Peer::handle_packet(char *data, int len) {
		char command[32];
		if(!find_param(0, data,(char *)&command, sizeof(command))) {
			m_delete_flag = true;
			return;
		}
		if(!strcmp("search", command)) {
			handle_search(data, len);
		} else if(!strcmp("others", command)) {
			handle_others(data, len);
		} else if(!strcmp("otherslist", command)) {
			handle_otherslist(data, len);
		} else if(!strcmp("valid", command)) {
			handle_valid(data, len);
		} else if(!strcmp("nicks", command)) {

		} else if(!strcmp("pmatch", command)) {

		} else if(!strcmp("check", command)) {
			handle_check(data, len);
		} else if(!strcmp("newuser", command)) {
			handle_newuser(data, len);
		} else if(!strcmp("uniquesearch", command)) {
			
		} else if(!strcmp("profilelist", command)) {
			
		}

		gettimeofday(&m_last_recv, NULL);
	}
	void Peer::m_newuser_cb(bool success, OS::User user, OS::Profile profile, OS::AuthData auth_data, void *extra) {
		int err_code = 0;
		if(auth_data.response_code != -1) {
			switch(auth_data.response_code) {
				case OS::CREATE_RESPONE_UNIQUENICK_IN_USE:
					err_code = GP_NEWUSER_UNIQUENICK_INUSE;
					break;
				case OS::LOGIN_RESPONSE_INVALID_PASSWORD:
					err_code = GP_NEWUSER_BAD_PASSWORD;
					break;
				case OS::CREATE_RESPONSE_INVALID_NICK:
					err_code = GP_NEWUSER_BAD_NICK;
				break;
				case OS::CREATE_RESPONSE_INVALID_UNIQUENICK:
					err_code = GP_NEWUSER_UNIQUENICK_INVALID;
				break;
			}
		}
		Peer *peer = (Peer *)extra;
		if(!g_gbl_sm_driver->HasPeer(peer))
			return;
		std::ostringstream s;
		s << "\\nur\\" << err_code;
		s << "\\pid\\" << profile.id;

		peer->SendPacket((const uint8_t *)s.str().c_str(),s.str().length());

		peer->m_delete_flag = true;
	}
	void Peer::handle_newuser(const char *buf, int len) {
		char nick[GP_NICK_LEN + 1];
		char uniquenick[GP_UNIQUENICK_LEN + 1];
		char email[GP_EMAIL_LEN + 1];
		char passenc[45 + 1];
		int partnercode = find_paramint("partnerid", (char *)buf);
		int namespaceid = find_paramint("namespaceid", (char *)buf);
		if(!find_param("email", (char*)buf, (char*)&email, GP_EMAIL_LEN)) {
			return;
		}
		if(!find_param("nick", (char*)buf, (char*)&nick, GP_NICK_LEN)) {
			return;
		}
		if(!find_param("uniquenick", (char*)buf, (char*)&uniquenick, GP_UNIQUENICK_LEN)) {
			uniquenick[0] = 0;
		}
		if(!find_param("passenc", (char*)buf, (char*)&passenc, sizeof(passenc) - 1)) {
			return;
		}
		int passlen = strlen(passenc);
		char *dpass = (char *)base64_decode((uint8_t *)passenc, &passlen);
		passlen = gspassenc((uint8_t *)dpass);

		OS::AuthTask::TryCreateUser_OrProfile(nick, uniquenick, namespaceid, email, partnercode, dpass, false, m_newuser_cb, this);
		if(dpass)
			free((void *)dpass);

	}
	void Peer::m_nick_email_auth_cb(bool success, OS::User user, OS::Profile profile, OS::AuthData auth_data, void *extra) {
		Peer *peer = (Peer *)extra;
		if(!g_gbl_sm_driver->HasPeer(peer))
			return;
		std::ostringstream s;
		s << "\\cur\\" << (int)success;
		s << "\\pid\\" << profile.id;

		peer->SendPacket((const uint8_t *)s.str().c_str(),s.str().length());

		peer->m_delete_flag = true;
	}
	void Peer::handle_check(const char *buf, int len) {

		char nick[GP_NICK_LEN + 1];
		char email[GP_EMAIL_LEN + 1];
		char passenc[45 + 1];
		int partnercode = find_paramint("partnerid", (char *)buf);
		if(!find_param("email", (char*)buf, (char*)&email, GP_EMAIL_LEN)) {
			return;
		}
		if(!find_param("nick", (char*)buf, (char*)&nick, GP_NICK_LEN)) {
			return;
		}
		if(!find_param("passenc", (char*)buf, (char*)&passenc, sizeof(passenc) - 1)) {
			return;
		}
		int passlen = strlen(passenc);
		char *dpass = (char *)base64_decode((uint8_t *)passenc, &passlen);
		passlen = gspassenc((uint8_t *)dpass);
	
		OS::AuthTask::TryAuthNickEmail(nick, email, partnercode, dpass, false, m_nick_email_auth_cb, this);

		if(dpass)
			free((void *)dpass);
	}
	void Peer::handle_search(const char *buf, int len) {
		OS::ProfileSearchRequest request;
		char temp[GP_REASON_LEN + 1];
		int temp_int;
		request.profile_search_details.id = 0;
		if(find_param("email", (char*)buf, (char*)&temp, GP_EMAIL_LEN)) {
			request.user_search_details.email = temp;
		}
		if(find_param("nick", (char*)buf, (char*)&temp, GP_NICK_LEN)) {
			request.profile_search_details.nick = temp;
		}
		if(find_param("uniquenick", (char*)buf, (char*)&temp, GP_UNIQUENICK_LEN)) {
			request.profile_search_details.uniquenick = temp;
		}
		if(find_param("firstname", (char*)buf, (char*)&temp, GP_FIRSTNAME_LEN)) {
			request.profile_search_details.firstname = temp;
		}
		if(find_param("lastname", (char*)buf, (char*)&temp, GP_LASTNAME_LEN)) {
			request.profile_search_details.lastname = temp;
		}
		temp_int = find_paramint("namespaceid", (char*)buf);
		if(temp_int != 0) {
			request.namespaceids.push_back(temp_int);
		}
		/*
		if(find_param("namespaceids", (char*)buf, (char*)&temp, GP_REASON_LEN)) {
			//TODO: namesiaceids\1,2,3,4,5
		}
		*/
		request.type = OS::EProfileSearch_Profiles;
		request.extra = this;
		request.callback = Peer::m_search_callback;
		OS::ProfileSearchTask::getProfileTask()->AddRequest(request);
	}
	void Peer::m_search_callback(bool success, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra) {
		Peer *peer = (Peer *)extra;
		if(!g_gbl_sm_driver->HasPeer(peer))
			return;
		std::ostringstream s;
		std::vector<OS::Profile>::iterator it = results.begin();
		while(it != results.end()) {
			OS::Profile p = *it;
			OS::User u = result_users[p.userid];
			s << "\\bsr\\" << p.id;
			s << "\\nick\\" << p.nick;
			s << "\\firstname\\" << p.firstname;
			s << "\\lastname\\" << p.lastname;
			s << "\\email\\" << u.email;
			s << "\\uniquenick\\" << p.uniquenick;
			s << "\\namespaceid\\" << p.namespaceid;
			it++;
		}
		s << "\\bsrdone\\";

		peer->SendPacket((const uint8_t *)s.str().c_str(),s.str().length());

		peer->m_delete_flag = true;
	}
	void Peer::m_search_buddies_callback(bool success, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra) {
		std::ostringstream s;
		Peer *peer = (Peer *)extra;
		if(!g_gbl_sm_driver->HasPeer(peer))
			return;
		std::vector<OS::Profile>::iterator it = results.begin();
		s << "\\otherslist\\";
		while(it != results.end()) {
			OS::Profile p = *it;
			OS::User u = result_users[p.userid];
			s << "\\o\\" << p.id;
			s << "\\nick\\" << p.nick;
			s << "\\first\\" << p.firstname;
			s << "\\last\\" << p.lastname;
			s << "\\email\\" << u.email;
			s << "\\uniquenick\\" << p.uniquenick;
			s << "\\namespaceid\\" << p.namespaceid;
			it++;
		}
		s << "\\oldone\\";

		peer->SendPacket((const uint8_t *)s.str().c_str(),s.str().length());

		peer->m_delete_flag = true;
	}
	void Peer::handle_others(const char *buf, int len) {
		OS::ProfileSearchRequest request;
		int profileid = find_paramint("profileid", (char *)buf);
		int namespaceid = find_paramint("namespaceid", (char *)buf);

		request.profile_search_details.id = profileid;
		request.namespaceids.push_back(namespaceid);

		request.type = OS::EProfileSearch_Buddies;
		request.extra = this;
		request.callback = Peer::m_search_buddies_callback;
		OS::ProfileSearchTask::getProfileTask()->AddRequest(request);
	}

	void Peer::m_search_buddies_reverse_callback(bool success, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra) {
		Peer *peer = (Peer *)extra;
		if(!g_gbl_sm_driver->HasPeer(peer))
			return;
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
			s << "\\email\\" << u.email;
			s << "\\uniquenick\\" << p.uniquenick;
			s << "\\namespaceid\\" << p.namespaceid;
			it++;
		}
		s << "\\odone\\";

		peer->SendPacket((const uint8_t *)s.str().c_str(),s.str().length());

		peer->m_delete_flag = true;
	}
	void Peer::handle_otherslist(const char *buf, int len) {
		char pid_buffer[256 + 1];
		OS::ProfileSearchRequest request;

		int profileid = find_paramint("profileid", (char *)buf);
		int namespaceid = find_paramint("namespaceid", (char *)buf);

		request.profile_search_details.id = profileid;
		request.namespaceids.push_back(namespaceid);

		if(find_param("opids", (char*)buf, (char*)&pid_buffer, sizeof(pid_buffer)-1)) {
			char *token;
			token = strtok(pid_buffer, "|");
			while(token != NULL) {
				request.target_profileids.push_back(atoi(token));
				token = strtok(NULL, "|");
			}
		}

		request.type = OS::EProfileSearch_Buddies_Reverse;
		request.extra = this;
		request.callback = Peer::m_search_buddies_reverse_callback;
		OS::ProfileSearchTask::getProfileTask()->AddRequest(request);
	}
	void Peer::m_search_valid_callback(bool success, std::vector<OS::User> results, void *extra) {
		Peer *peer = (Peer *)extra;
		if(!g_gbl_sm_driver->HasPeer(peer))
			return;
		std::ostringstream s;

		s << "\\vr\\" << ((results.size() > 0) ? 1 : 0) << "\\final\\";

		peer->SendPacket((const uint8_t *)s.str().c_str(),s.str().length());

		peer->m_delete_flag = true;
	}
	void Peer::handle_valid(const char *buf, int len) {
		OS::UserSearchRequest request;
		char email_buffer[GP_EMAIL_LEN + 1];
		request.userid = find_paramint("userid", (char *)buf);
		request.partnercode = find_paramint("partnercode", (char *)buf);
		if(find_param("email", (char*)buf, (char*)&email_buffer, sizeof(email_buffer)-1)) {
			request.email = email_buffer;
		}
		request.extra = this;
		request.callback = Peer::m_search_valid_callback;
		OS::UserSearchTask::getUserTask()->AddRequest(request);
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
}