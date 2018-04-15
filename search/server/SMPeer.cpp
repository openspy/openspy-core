#include "SMPeer.h"
#include "SMDriver.h"
#include <OS/OpenSpy.h>
#include <OS/Buffer.h>
#include <OS/Search/Profile.h>
#include <OS/legacy/helpers.h>
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
	const char *Peer::mp_hidden_str = "[hidden]";
	Peer::Peer(Driver *driver, INetIOSocket *sd) : INetPeer(driver, sd) {
		m_delete_flag = false;
		m_timeout_flag = false;
		mp_mutex = OS::CreateMutex();
		ResetMetrics();
		gettimeofday(&m_last_ping, NULL);
		OS::LogText(OS::ELogLevel_Info, "[%s] New connection", m_sd->address.ToString().c_str());
	}
	Peer::~Peer() {
		OS::LogText(OS::ELogLevel_Info, "[%s] Connection closed, timeout: %d", m_sd->address.ToString().c_str(), m_timeout_flag);
		delete mp_mutex;
	}
	void Peer::think(bool packet_waiting) {
		int len = 0;
		NetIOCommResp io_resp;
		if (m_delete_flag) return;

		if (packet_waiting) {
			io_resp = this->GetDriver()->getServer()->getNetIOInterface()->streamRecv(m_sd, m_recv_buffer);

			if (io_resp.disconnect_flag || io_resp.error_flag) {
				goto end;
			}

			len = m_recv_buffer.size();

			std::string recv_buf((const char *)m_recv_buffer.GetHead(), len);

			m_peer_stats.packets_in++;
			m_peer_stats.bytes_in += len;

			/* split by \\final\\  */
			char *p = (char *)recv_buf.c_str();
			char *x;
			while (true) {
				x = p;
				p = strstr(p, "\\final\\");
				if (p == NULL) { break; }
				*p = 0;
				this->handle_packet(x, strlen(x));
				p += 7;
			}
			this->handle_packet(x, strlen(x));
		}

	end:
		//send_ping();

		//check for timeout
		struct timeval current_time;
		gettimeofday(&current_time, NULL);
		if (current_time.tv_sec - m_last_recv.tv_sec > SM_PING_TIME * 2) {
			Delete(true);
		}
		else if (len <= 0 && packet_waiting) {
			Delete();
		}
	}
	void Peer::handle_packet(char *data, int len) {
		char command[32];
		if(!find_param(0, data,(char *)&command, sizeof(command))) {
			Delete();
			return;
		}
		printf("Handle: %s\n", data);

		OS::KVReader data_parser = OS::KVReader(data);
		if(!strcmp("search", command)) {
			handle_search(data_parser);
		} else if(!strcmp("others", command)) {
			handle_others(data_parser);
		} else if(!strcmp("otherslist", command)) {
			handle_otherslist(data_parser);
		} else if(!strcmp("valid", command)) {
			handle_valid(data_parser);
		} else if(!strcmp("nicks", command)) {
			handle_nicks(data_parser);
		} else if(!strcmp("pmatch", command)) {

		} else if(!strcmp("check", command)) {
			handle_check(data_parser);
		} else if(!strcmp("newuser", command)) {
			handle_newuser(data_parser);
		} else if(!strcmp("uniquesearch", command)) {
			
		} else if(!strcmp("profilelist", command)) {
			
		}

		gettimeofday(&m_last_recv, NULL);
	}
	void Peer::m_newuser_cb(bool success, OS::User user, OS::Profile profile, OS::AuthData auth_data, void *extra, int operation_id, INetPeer *peer) {
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
		std::ostringstream s;
		s << "\\nur\\" << err_code;
		s << "\\pid\\" << profile.id;

		((Peer *)peer)->SendPacket((const uint8_t *)s.str().c_str(),s.str().length());

		((Peer *)peer)->Delete();
	}
	void Peer::handle_newuser(OS::KVReader data_parser) {
		std::string nick;
		std::string uniquenick;
		std::string email;
		std::string passenc;
		int partnercode = data_parser.GetValueInt("partnerid");
		int namespaceid = data_parser.GetValueInt("namespaceid");

		if (!data_parser.HasKey("email") || !data_parser.HasKey("nick") || !data_parser.HasKey("passenc")) {
			return;
		}

		email = data_parser.GetValue("email");
		nick = data_parser.GetValue("nick");
		passenc = data_parser.GetValue("passenc");


		if (data_parser.HasKey("uniquenick")) {
			uniquenick = data_parser.GetValue("uniquenick");
		}

		int passlen = passenc.length();
		char *dpass = (char *)base64_decode((uint8_t *)passenc.c_str(), &passlen);
		passlen = gspassenc((uint8_t *)dpass);

		OS::AuthTask::TryCreateUser_OrProfile(nick, uniquenick, namespaceid, email, partnercode, dpass, false, m_newuser_cb, this, 0, this);
		if(dpass)
			free((void *)dpass);

	}
	void Peer::m_nick_email_auth_cb(bool success, OS::User user, OS::Profile profile, OS::AuthData auth_data, void *extra, int operation_id, INetPeer *peer) {
		std::ostringstream s;
		s << "\\cur\\" << (int)success;
		s << "\\pid\\" << profile.id;

		((Peer *)peer)->SendPacket((const uint8_t *)s.str().c_str(),s.str().length());

		((Peer *)peer)->Delete();
	}
	void Peer::handle_check(OS::KVReader data_parser) {

		std::string nick;
		std::string uniquenick;
		std::string email;
		std::string passenc;
		int partnercode = data_parser.GetValueInt("partnerid");
		int namespaceid = data_parser.GetValueInt("namespaceid");

		if (!data_parser.HasKey("email") || !data_parser.HasKey("nick") || !data_parser.HasKey("passenc")) {
			return;
		}

		email = data_parser.GetValue("email");
		nick = data_parser.GetValue("nick");
		passenc = data_parser.GetValue("passenc");


		if (data_parser.HasKey("uniquenick")) {
			uniquenick = data_parser.GetValue("uniquenick");
		}

		int passlen = passenc.length();
		char *dpass = (char *)base64_decode((uint8_t *)passenc.c_str(), &passlen);
		passlen = gspassenc((uint8_t *)dpass);
	
		OS::AuthTask::TryAuthNickEmail(nick, email, partnercode, dpass, false, m_nick_email_auth_cb, this, 0, this);

		if(dpass)
			free((void *)dpass);
	}
	void Peer::handle_search(OS::KVReader data_parser) {
		OS::ProfileSearchRequest request;
		char temp[GP_REASON_LEN + 1];
		int temp_int;
		request.profile_search_details.id = 0;
		if(data_parser.HasKey("email")) {
			request.user_search_details.email = data_parser.GetValue("email");
		}
		if(data_parser.HasKey("nick")) {
			request.profile_search_details.nick = data_parser.GetValue("nick");
		}
		if(data_parser.HasKey("uniquenick")) {
			request.profile_search_details.uniquenick = data_parser.GetValue("uniquenick");
		}
		if(data_parser.HasKey("firstname")) {
			request.profile_search_details.firstname = data_parser.GetValue("firstname");
		}
		if (data_parser.HasKey("lastname")) {
			request.profile_search_details.lastname = data_parser.GetValue("lastname");
		}
		temp_int = data_parser.GetValueInt("namespaceid");
		if (temp_int != 0) {
			request.namespaceids.push_back(temp_int);
		}

		request.user_search_details.partnercode = data_parser.GetValueInt("partnerid");
		/*
		if(find_param("namespaceids", (char*)buf, (char*)&temp, GP_REASON_LEN)) {
			//TODO: namesiaceids\1,2,3,4,5
		}
		*/
		request.type = OS::EProfileSearch_Profiles;
		request.extra = this;
		request.peer = this;
		IncRef();
		request.callback = Peer::m_search_callback;
		OS::m_profile_search_task_pool->AddRequest(request);
	}
	void Peer::m_search_callback(OS::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer) {
		std::ostringstream s;
		std::vector<OS::Profile>::iterator it = results.begin();
		while(it != results.end()) {
			OS::Profile p = *it;
			OS::User u = result_users[p.userid];
			s << "\\bsr\\" << p.id;
			s << "\\nick\\" << p.nick;
			s << "\\firstname\\" << p.firstname;
			s << "\\lastname\\" << p.lastname;
			s << "\\email\\";
			if(u.publicmask & GP_MASK_EMAIL)
				s << u.email;
			else 
				s << Peer::mp_hidden_str;
			s << "\\uniquenick\\" << p.uniquenick;
			s << "\\namespaceid\\" << p.namespaceid;
			it++;
		}
		s << "\\bsrdone\\";
		((Peer *)peer)->SendPacket((const uint8_t *)s.str().c_str(),s.str().length());

		((Peer *)peer)->Delete();
	}
	void Peer::m_search_buddies_callback(OS::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer) {
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
			s << "\\email\\";
			if(u.publicmask & GP_MASK_EMAIL)
				s << u.email;
			else 
				s << Peer::mp_hidden_str;
			s << "\\uniquenick\\" << p.uniquenick;
			s << "\\namespaceid\\" << p.namespaceid;
			it++;
		}
		s << "\\oldone\\";

		((Peer *)peer)->SendPacket((const uint8_t *)s.str().c_str(),s.str().length());

		((Peer *)peer)->Delete();
	}
	void Peer::handle_others(OS::KVReader data_parser) {
		OS::ProfileSearchRequest request;
		int profileid = data_parser.GetValueInt("profileid");
		int namespaceid = data_parser.GetValueInt("namespaceid");

		request.profile_search_details.id = profileid;
		request.namespaceids.push_back(namespaceid);

		request.type = OS::EProfileSearch_Buddies;
		request.extra = this;
		request.peer = this;
		IncRef();
		request.callback = Peer::m_search_buddies_callback;
		OS::m_profile_search_task_pool->AddRequest(request);
	}

	void Peer::m_search_buddies_reverse_callback(OS::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer) {
		std::ostringstream s;
		std::vector<OS::Profile>::iterator it = results.begin();
		s << "\\others\\";
		while(it != results.end()) {
			OS::Profile p = *it;
			OS::User u = result_users[p.userid];
			if(~u.publicmask & GP_MASK_BUDDYLIST) {
				s << "\\o\\" << p.id;
				s << "\\nick\\" << p.nick;
				s << "\\first\\" << p.firstname;
				s << "\\last\\" << p.lastname;
				s << "\\email\\";
				if(u.publicmask & GP_MASK_EMAIL)
					s << u.email;
				else 
					s << Peer::mp_hidden_str;
				s << "\\uniquenick\\" << p.uniquenick;
				s << "\\namespaceid\\" << p.namespaceid;
			}
			it++;
		}
		s << "\\odone\\";

		((Peer *)peer)->SendPacket((const uint8_t *)s.str().c_str(),s.str().length());

		((Peer *)peer)->Delete();
	}
	void Peer::handle_otherslist(OS::KVReader data_parser) {
		std::string pid_buffer;
		OS::ProfileSearchRequest request;

		int profileid = data_parser.GetValueInt("profileid");
		int namespaceid = data_parser.GetValueInt("namespaceid");

		request.profile_search_details.id = profileid;
		request.namespaceids.push_back(namespaceid);

		if(data_parser.HasKey("opids")) {
			char *pid_buffer = strdup(data_parser.GetValue("opids").c_str());
			char *token;
			token = strtok(pid_buffer, "|");
			while(token != NULL) {
				request.target_profileids.push_back(atoi(token));
				token = strtok(NULL, "|");
			}

			free((void *)pid_buffer);
		}

		request.type = OS::EProfileSearch_Buddies_Reverse;
		request.extra = this;
		request.peer = this;
		IncRef();
		request.callback = Peer::m_search_buddies_reverse_callback;
		OS::m_profile_search_task_pool->AddRequest(request);
	}
	void Peer::m_search_valid_callback(OS::EUserResponseType response_type, std::vector<OS::User> results, void *extra, INetPeer *peer) {
		std::ostringstream s;

		s << "\\vr\\" << ((results.size() > 0) ? 1 : 0) << "\\final\\";

		((Peer *)peer)->SendPacket((const uint8_t *)s.str().c_str(),s.str().length());

		((Peer *)peer)->Delete();
	}
	void Peer::handle_valid(OS::KVReader data_parser) {
		OS::UserSearchRequest request;
		request.type = OS::EUserRequestType_Search;
		if (data_parser.HasKey("userid")) {
			request.search_params.id = data_parser.GetValueInt("userid");
		}
		
		if (data_parser.HasKey("partnercode")) {
			request.search_params.partnercode = data_parser.GetValueInt("partnercode");
		}
		else if(data_parser.HasKey("partnerid")) {
			request.search_params.partnercode = data_parser.GetValueInt("partnerid");
		}

		if(data_parser.HasKey("email")) {
			request.search_params.email = data_parser.GetValue("email");
		}
		request.extra = this;
		request.peer = this;
		IncRef();
		request.callback = Peer::m_search_valid_callback;
		OS::m_user_search_task_pool->AddRequest(request);
	}

	void Peer::handle_nicks(OS::KVReader data_parser) {
		OS::ProfileSearchRequest request;
		request.type = OS::EProfileSearch_Profiles;
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

		if (!data_parser.HasKey("passenc")) {
			Delete();
			return;
		}

		std::string passenc = data_parser.GetValue("passenc");

		int passlen = passenc.length();
		char *dpass = (char *)base64_decode((uint8_t *)passenc.c_str(), &passlen);
		passlen = gspassenc((uint8_t *)dpass);

		request.user_search_details.password = dpass;

		if (dpass)
			free((void *)dpass);

		request.extra = this;
		request.peer = this;
		IncRef();
		request.callback = Peer::m_nicks_cb;
		OS::m_profile_search_task_pool->AddRequest(request);
	}
	void Peer::m_nicks_cb(OS::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer) {
		std::ostringstream s;

		s << "\\nr\\" << results.size();

		std::vector<OS::Profile>::iterator it = results.begin();
		while (it != results.end()) {
			OS::Profile p = *it;
			if(p.nick.length())
				s << "\\nick\\" << p.nick;

			if(p.uniquenick.length())
				s << "\\uniquenick\\" << p.uniquenick;
			it++;
		}

		s << "\\ndone\\";

		((Peer *)peer)->SendPacket((const uint8_t *)s.str().c_str(), s.str().length());
	}
	void Peer::SendPacket(const uint8_t *buff, int len, bool attach_final) {
		OS::Buffer buffer;
		buffer.WriteBuffer((void *)buff, len);
		if (attach_final) {
			buffer.WriteBuffer((void *)"\\final\\", 7);
		}
		m_peer_stats.bytes_out += buffer.size();
		m_peer_stats.packets_out++;
		NetIOCommResp io_resp;
		io_resp = this->GetDriver()->getServer()->getNetIOInterface()->streamSend(m_sd, buffer);
		if (io_resp.disconnect_flag || io_resp.error_flag) {
			Delete();
		}
	}
	OS::MetricValue Peer::GetMetricItemFromStats(PeerStats stats) {
		OS::MetricValue arr_value, value;
		value.value._str = stats.m_address.ToString(false);
		value.key = "ip";
		value.type = OS::MetricType_String;
		arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_String, value));

		value.value._int = stats.disconnected;
		value.key = "disconnected";
		value.type = OS::MetricType_Integer;
		arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_String, value));

		value.type = OS::MetricType_Integer;
		value.value._int = stats.bytes_in;
		value.key = "bytes_in";
		arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_String, value));

		value.value._int = stats.bytes_out;
		value.key = "bytes_out";
		arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_String, value));

		value.value._int = stats.packets_in;
		value.key = "packets_in";
		arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_String, value));

		value.value._int = stats.packets_out;
		value.key = "packets_out";
		arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_String, value));

		value.value._int = stats.total_requests;
		value.key = "pending_requests";
		arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_String, value));
		arr_value.type = OS::MetricType_Array;

		value.type = OS::MetricType_String;
		value.key = "gamename";
		value.value._str = stats.from_game.gamename;
		arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_String, value));

		value.type = OS::MetricType_Integer;
		value.key = "version";
		value.value._int = stats.version;
		arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_String, value));

		arr_value.key = stats.m_address.ToString(false);
		return arr_value;
	}
	void Peer::ResetMetrics() {
		m_peer_stats.bytes_in = 0;
		m_peer_stats.bytes_out = 0;
		m_peer_stats.packets_in = 0;
		m_peer_stats.packets_out = 0;
		m_peer_stats.total_requests = 0;
	}
	OS::MetricInstance Peer::GetMetrics() {
		OS::MetricInstance peer_metric;

		peer_metric.value = GetMetricItemFromStats(m_peer_stats);
		peer_metric.key = "peer";

		ResetMetrics();

		return peer_metric;
	}
	void Peer::Delete(bool timeout) {
		m_timeout_flag = timeout;
		m_delete_flag = true;
	}
}