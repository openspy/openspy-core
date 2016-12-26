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

#include <sstream>

namespace SM {

	Peer::Peer(Driver *driver, struct sockaddr_in *address_info, int sd) {
		m_sd = sd;
		mp_driver = driver;
		m_address_info = *address_info;
		m_delete_flag = false;
		m_timeout_flag = false;
		gettimeofday(&m_last_ping, NULL);
		printf("SM New peer\n");
	}
	Peer::~Peer() {
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
 		if(len == 0 && packet_waiting) {
			m_delete_flag = true;
		}
	}
	void Peer::handle_packet(char *data, int len) {
		printf("SM Handle(%d): %s\n", len,data);
		char command[32];
		if(!find_param(0, data,(char *)&command, sizeof(command))) {
			m_delete_flag = true;
			return;
		}
		printf("Got cmd: %s\n", command);
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

		} else if(!strcmp("newuser", command)) {
			
		} else if(!strcmp("uniquesearch", command)) {
			
		} else if(!strcmp("profilelist", command)) {
			
		}

		gettimeofday(&m_last_recv, NULL);
	}
	void Peer::handle_search(const char *buf, int len) {
		OS::ProfileSearchRequest request;
		char temp[GP_REASON_LEN + 1];
		int temp_int;
		request.profileid = 0;
		if(find_param("email", (char*)buf, (char*)&temp, GP_EMAIL_LEN)) {
			request.email = temp;
		}
		if(find_param("nick", (char*)buf, (char*)&temp, GP_NICK_LEN)) {
			request.nick = temp;
		}
		if(find_param("uniquenick", (char*)buf, (char*)&temp, GP_UNIQUENICK_LEN)) {
			request.uniquenick = temp;
		}
		if(find_param("firstname", (char*)buf, (char*)&temp, GP_FIRSTNAME_LEN)) {
			request.firstname = temp;
		}
		if(find_param("lastname", (char*)buf, (char*)&temp, GP_LASTNAME_LEN)) {
			request.lastname = temp;
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

		((Peer *)extra)->SendPacket((const uint8_t *)s.str().c_str(),s.str().length());

		((Peer *)extra)->m_delete_flag = true;
	}
	void Peer::m_search_buddies_callback(bool success, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra) {
		std::ostringstream s;
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

		((Peer *)extra)->SendPacket((const uint8_t *)s.str().c_str(),s.str().length());

		((Peer *)extra)->m_delete_flag = true;
	}
	void Peer::handle_others(const char *buf, int len) {
		OS::ProfileSearchRequest request;
		int profileid = find_paramint("profileid", (char *)buf);
		int namespaceid = find_paramint("namespaceid", (char *)buf);

		request.profileid = profileid;
		request.namespaceids.push_back(namespaceid);

		request.type = OS::EProfileSearch_Buddies;
		request.extra = this;
		request.callback = Peer::m_search_buddies_callback;
		OS::ProfileSearchTask::getProfileTask()->AddRequest(request);
	}

	void Peer::m_search_buddies_reverse_callback(bool success, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra) {
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

		((Peer *)extra)->SendPacket((const uint8_t *)s.str().c_str(),s.str().length());

		((Peer *)extra)->m_delete_flag = true;
	}
	void Peer::handle_otherslist(const char *buf, int len) {
		char pid_buffer[256 + 1];
		OS::ProfileSearchRequest request;

		int profileid = find_paramint("profileid", (char *)buf);
		int namespaceid = find_paramint("namespaceid", (char *)buf);

		request.profileid = profileid;
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
		std::ostringstream s;

		s << "\\vr\\" << ((results.size() > 0) ? 1 : 0) << "\\final\\";

		((Peer *)extra)->SendPacket((const uint8_t *)s.str().c_str(),s.str().length());

		((Peer *)extra)->m_delete_flag = true;
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