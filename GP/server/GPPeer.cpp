#include "GPPeer.h"
#include "GPDriver.h"
#include <OS/OpenSpy.h>
#include <OS/Search/Profile.h>
#include <OS/legacy/helpers.h>
#include <OS/legacy/buffreader.h>
#include <OS/legacy/buffwriter.h>
#include <OS/socketlib/socketlib.h>

#include <OS/Search/User.h>

#include <sstream>
#include <algorithm>

#include "GPBackend.h"

using namespace GPShared;

namespace GP {

	Peer::Peer(Driver *driver, struct sockaddr_in *address_info, int sd) {
		m_sd = sd;
		mp_driver = driver;
		m_address_info = *address_info;
		m_delete_flag = false;
		m_timeout_flag = false;
		mp_mutex = OS::CreateMutex();
		gettimeofday(&m_last_ping, NULL);

		m_status.status = GP_OFFLINE;
		m_status.status_str[0] = 0;
		m_status.location_str[0] = 0;
		m_status.quiet_flags = GP_SILENCE_NONE;
		m_status.address.ip = address_info->sin_addr.s_addr;
		m_status.address.port = address_info->sin_port;

		gen_random(m_challenge, CHALLENGE_LEN);

		send_login_challenge(1);
	}
	Peer::~Peer() {
		delete mp_mutex;
		close(m_sd);
	}
	void Peer::think(bool packet_waiting) {
		char buf[GPI_READ_SIZE + 1];
		socklen_t slen = sizeof(struct sockaddr_in);
		int len = 0, piece_len = 0;
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
		} else if(len == 0 && packet_waiting) {
			m_delete_flag = true;
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
		if(m_backend_session_key.length() > 0) {
			if(!strcmp(command, "status")) {
				handle_status(data, len);
			} else if(!strcmp(command, "addbuddy")) {
				handle_addbuddy(data, len);
			} else if(!strcmp(command, "delbuddy")) {
				handle_delbuddy(data, len);
			} else if(!strcmp(command, "addblock")) {
				handle_addblock(data, len);
			} else if(!strcmp(command, "removeblock")) {
				handle_removeblock(data, len);
			} else if(!strcmp(command, "revoke")) {
				handle_revoke(data, len);
			} else if(!strcmp(command, "authadd")) {
				handle_authadd(data, len);
			} else if(!strcmp(command, "getprofile")) {
				handle_getprofile(data, len);
			} else if(!strcmp(command, "bm")) {
				handle_bm(data, len);
			} else if(!strcmp(command, "pinvite")) {
				handle_pinvite(data, len);
			} else if(!strcmp(command, "newprofile")) {
				handle_newprofile(data, len);
			} else if(!strcmp(command, "delprofile")) {
				handle_delprofile(data, len);
			} else if(!strcmp(command, "registernick")) {
				handle_registernick(data, len);
			} else if(!strcmp(command, "registercdkey")) {
				handle_registercdkey(data, len);
			} else if(!strcmp(command, "updatepro")) {
				handle_updatepro(data, len);
			}
		}
	}
	void Peer::m_update_profile_callback(OS::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra) {
	}
	/*
		Updates profile specific information
	*/
	void Peer::handle_updatepro(const char *data, int len) {
		bool send_userupdate = false;
		char buff[GP_STATUS_STRING_LEN + 1];
		if(find_param("zipcode", (char *)data, (char *)&buff, GP_STATUS_STRING_LEN)) {
			m_profile.zipcode = atoi(buff);
		}
		if(find_param("sex", (char *)data, (char *)&buff, GP_STATUS_STRING_LEN)) {
			m_profile.sex = atoi(buff);
		}
		if(find_param("pic", (char *)data, (char *)&buff, GP_STATUS_STRING_LEN)) {
			m_profile.pic = atoi(buff);
		}
		if(find_param("ooc", (char *)data, (char *)&buff, GP_STATUS_STRING_LEN)) {
			m_profile.ooc = atoi(buff);
		}
		if(find_param("ind", (char *)data, (char *)&buff, GP_STATUS_STRING_LEN)) {
			m_profile.ind = atoi(buff);
		}
		if(find_param("mar", (char *)data, (char *)&buff, GP_STATUS_STRING_LEN)) {
			m_profile.mar = atoi(buff);
		}
		if(find_param("chc", (char *)data, (char *)&buff, GP_STATUS_STRING_LEN)) {
			m_profile.chc = atoi(buff);
		}
		if(find_param("i1", (char *)data, (char *)&buff, GP_STATUS_STRING_LEN)) {
			m_profile.i1 = atoi(buff);
		}
		if(find_param("birthday", (char *)data, (char *)&buff, GP_STATUS_STRING_LEN)) {
			m_profile.birthday = atoi(buff);
		}
		if(find_param("publicmask", (char *)data, (char *)&buff, GP_STATUS_STRING_LEN)) {
			send_userupdate = true;
			m_user.publicmask = atoi(buff);
			printf("GOt punlicmask: %d\n", m_user.publicmask);
		}

		if(find_param("nick", (char *)data, (char *)&buff, GP_STATUS_STRING_LEN)) {
			m_profile.nick = buff;
		}

		if(find_param("uniquenick", (char *)data, (char *)&buff, GP_STATUS_STRING_LEN)) {
			m_profile.uniquenick = buff;
		}
		if(find_param("firstname", (char *)data, (char *)&buff, GP_STATUS_STRING_LEN)) {
			m_profile.firstname = buff;
		}
		if(find_param("lastname", (char *)data, (char *)&buff, GP_STATUS_STRING_LEN)) {
			m_profile.lastname = buff;
		}
		if(find_param("countrycode", (char *)data, (char *)&buff, GP_STATUS_STRING_LEN)) {
			m_profile.countrycode = buff;
		}
		if(find_param("videocard1string", (char *)data, (char *)&buff, GP_STATUS_STRING_LEN)) {
			m_profile.videocardstring[0] = buff;
		}
		if(find_param("videocard2string", (char *)data, (char *)&buff, GP_STATUS_STRING_LEN)) {
			m_profile.videocardstring[1] = buff;
		}
		if(find_param("osstring", (char *)data, (char *)&buff, GP_STATUS_STRING_LEN)) {
			m_profile.osstring = buff;
		}
		if(find_param("aim", (char *)data, (char *)&buff, GP_STATUS_STRING_LEN)) {
			m_profile.aim = buff;
		}

		OS::ProfileSearchRequest request;
		request.profile_search_details = m_profile;
		request.extra = this;
		request.type = OS::EProfileSearch_UpdateProfile;
		request.callback = Peer::m_update_profile_callback;
		OS::ProfileSearchTask::getProfileTask()->AddRequest(request);

		if(send_userupdate) {
			OS::UserSearchRequest user_request;
			user_request.search_params = m_user;
			user_request.type = OS::EUserRequestType_Update;
			user_request.extra = this;
			user_request.callback = NULL;
			user_request.search_params = m_user;
			OS::UserSearchTask::getUserTask()->AddRequest(user_request);
		}
	}
	void Peer::handle_updateui(const char *data, int len) {
		char buff[GP_STATUS_STRING_LEN + 1];
		OS::UserSearchRequest request;
		if(find_param("cpubrandid", (char *)data, (char *)&buff, GP_STATUS_STRING_LEN)) {
			m_user.cpu_brandid = atoi(buff);
		}

		if(find_param("cpuspeed", (char *)data, (char *)&buff, GP_STATUS_STRING_LEN)) {
			m_user.cpu_speed = atoi(buff);
		}

		if(find_param("vidocard1ram", (char *)data, (char *)&buff, GP_STATUS_STRING_LEN)) {
			m_user.videocard_ram[0] = atoi(buff);
		}

		if(find_param("vidocard2ram", (char *)data, (char *)&buff, GP_STATUS_STRING_LEN)) {
			m_user.videocard_ram[1] = atoi(buff);
		}

		if(find_param("connectionid", (char *)data, (char *)&buff, GP_STATUS_STRING_LEN)) {
			m_user.connectionid = atoi(buff);
		}

		if(find_param("connectionspeed", (char *)data, (char *)&buff, GP_STATUS_STRING_LEN)) {
			m_user.connectionspeed = atoi(buff);
		}

		if(find_param("hasnetwork", (char *)data, (char *)&buff, GP_STATUS_STRING_LEN)) {
			m_user.hasnetwork = atoi(buff);
		}

		if(find_param("email", (char *)data, (char *)&buff, GP_STATUS_STRING_LEN)) {
			m_user.email = buff;
		}

		if(find_param("passwordenc", (char *)data, (char *)&buff, GP_STATUS_STRING_LEN)) {
			int passlen = strlen(buff);
			char *dpass = (char *)base64_decode((uint8_t *)buff, &passlen);
			passlen = gspassenc((uint8_t *)dpass);

			if(dpass)
				m_user.password = dpass;

			if(dpass)
				free((void *)dpass);
		}
		
		request.search_params = m_user;
		request.type = OS::EUserRequestType_Update;
		request.extra = this;
		request.callback = NULL;
		request.search_params = m_user;
		OS::UserSearchTask::getUserTask()->AddRequest(request);
	}
	void Peer::handle_registernick(const char *data, int len) {
		
	}
	void Peer::handle_registercdkey(const char *data, int len) {

	}
	void Peer::m_create_profile_callback(OS::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra) {
		if(response_reason == OS::EProfileResponseType_Success && results.size() > 0) {
			OS::Profile profile = results.front();
		}
	}
	void Peer::handle_newprofile(const char *data, int len) {
		OS::ProfileSearchRequest request;
		char nick[GP_NICK_LEN + 1], oldnick[GP_NICK_LEN + 1];
		bool replace = find_paramint("replace", (char *)data) != 0;
		find_param("nick",(char *)data, (char *)&nick,GP_NICK_LEN);
		if(replace) { //TODO: figure out replaces functionality
			find_param("oldnick",(char *)data, (char *)&oldnick,GP_NICK_LEN);
		}
		request.profile_search_details.id = m_profile.id;
		request.extra = this;
		request.type = OS::EProfileSearch_CreateProfile;
		request.callback = Peer::m_create_profile_callback;
		OS::ProfileSearchTask::getProfileTask()->AddRequest(request);
	}
	void Peer::m_delete_profile_callback(OS::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra) {
		std::ostringstream s;
		Peer *peer = (Peer *)extra;
		if(!g_gbl_gp_driver->HasPeer(peer))
			return;

		s << "\\dpr\\" << (int)(response_reason == OS::EProfileResponseType_Success);
		peer->SendPacket((const uint8_t *)s.str().c_str(),s.str().length());
	}
	void Peer::handle_delprofile(const char *data, int len) {
		OS::ProfileSearchRequest request;
		request.profile_search_details.id = m_profile.id;
		request.extra = this;
		request.type = OS::EProfileSearch_DeleteProfile;
		request.callback = Peer::m_delete_profile_callback;
		OS::ProfileSearchTask::getProfileTask()->AddRequest(request);
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

		int operation_id = find_paramint("id",(char *)data);		
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

		if(type == 1) {
			perform_nick_email_auth(user, partnercode, m_challenge, challenge, response, operation_id);
		}
	}
	void Peer::handle_pinvite(const char *data, int len) {
		//profileid\10000\productid\1
		std::ostringstream s;
		int profileid = find_paramint("profileid", (char *)data);
		int productid = find_paramint("productid", (char *)data);

		s << "|p|" << productid;
		s << "|l|" << m_status.location_str;
		s << "|signed|d41d8cd98f00b204e9800998ecf8427e"; //temp until calculation fixed
		GPBackend::GPBackendRedisTask::SendMessage(this, profileid, GPI_BM_INVITE, s.str().c_str());
	}
	void Peer::handle_status(const char *data, int len) {
		m_status.status = (GPEnum)find_paramint("status",(char *)data);

		find_param("statstring",(char *)data, (char *)&m_status.status_str,GP_STATUS_STRING_LEN);
		find_param("locstring",(char *)data, (char *)&m_status.location_str,GP_LOCATION_STRING_LEN);

		GPBackend::GPBackendRedisTask::SetPresenceStatus(m_profile.id, m_status, this);
	}
	void Peer::handle_statusinfo(const char *data, int len) {

	}
	void Peer::handle_addbuddy(const char *data, int len) {
		int newprofileid = find_paramint("newprofileid",(char *)data);
		char reason[GP_REASON_LEN + 1];
		find_param("reason",(char *)data, (char *)&reason,GP_REASON_LEN);
		mp_mutex->lock();
		if(m_buddies.find(newprofileid) != m_buddies.end()) {
			mp_mutex->unlock();
			send_error(GP_ADDBUDDY_ALREADY_BUDDY);
			return;
		} else if(std::find(m_blocks.begin(),m_blocks.end(), newprofileid) == m_blocks.end()) {
			mp_mutex->unlock();
			send_error(GP_ADDBUDDY_IS_ON_BLOCKLIST);
			return;
		}
		mp_mutex->unlock();

		GPBackend::GPBackendRedisTask::MakeBuddyRequest(m_profile.id, newprofileid, reason);
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
	void Peer::m_buddy_list_lookup_callback(OS::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra) {
		std::ostringstream s;
		std::string str;
		std::vector<OS::Profile>::iterator it = results.begin();
		Peer *peer = (Peer *)extra;
		if(!g_gbl_gp_driver->HasPeer(peer))
			return;
		peer->mp_mutex->lock();
		if(results.size() > 0) {
			s << "\\bdy\\" << results.size();
			s << "\\list\\";
			while(it != results.end()) {
				OS::Profile p = *it;
				s << p.id << ",";

				if(peer->m_buddies.find(p.id) == peer->m_buddies.end())
					peer->m_buddies[p.id] = GPShared::gp_default_status;

				it++;
			}
			str = s.str();
			str = str.substr(0, str.size()-1);
			peer->SendPacket((const uint8_t *)str.c_str(),str.length());
		}
		peer->mp_mutex->unlock();
	}
	void Peer::m_block_list_lookup_callback(OS::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra) {
		std::ostringstream s;
		std::string str;
		std::vector<OS::Profile>::iterator it = results.begin();
		Peer *peer = (Peer *)extra;
		if(!g_gbl_gp_driver->HasPeer(peer))
			return;
		peer->mp_mutex->lock();
		if(results.size() > 0) {
			s << "\\blk\\" << results.size();
			s << "\\list\\";
			while(it != results.end()) {
				OS::Profile p = *it;
				s << p.id << ",";
				peer->m_blocks.push_back(p.id);
				it++;
			}
			str = s.str();
			str = str.substr(0, str.size()-1);
			peer->SendPacket((const uint8_t *)str.c_str(),str.length());
		}
		peer->mp_mutex->unlock();
	}
	void Peer::send_buddies() {
		OS::ProfileSearchRequest request;
		request.profile_search_details.id = m_profile.id;
		request.extra = this;
		request.type = OS::EProfileSearch_Buddies;
		request.callback = Peer::m_buddy_list_lookup_callback;
		OS::ProfileSearchTask::getProfileTask()->AddRequest(request);
	}
	void Peer::send_blocks() {
		OS::ProfileSearchRequest request;
		request.profile_search_details.id = m_profile.id;
		request.extra = this;
		request.type = OS::EProfileSearch_Blocks;
		request.callback = Peer::m_block_list_lookup_callback;
		OS::ProfileSearchTask::getProfileTask()->AddRequest(request);
	}
	void Peer::handle_delbuddy(const char *data, int len) {
		int delprofileid = find_paramint("delprofileid",(char *)data);
		GPBackend::GPBackendRedisTask::MakeDelBuddyRequest(m_profile.id, delprofileid);
	}
	void Peer::handle_revoke(const char *data, int len) {
		int delprofileid = find_paramint("profileid",(char *)data);
		GPBackend::GPBackendRedisTask::MakeRevokeAuthRequest(delprofileid, m_profile.id);
	}
	void Peer::handle_authadd(const char *data, int len) {
		int fromprofileid = find_paramint("fromprofileid",(char *)data);
		GPBackend::GPBackendRedisTask::MakeAuthorizeBuddyRequest(m_profile.id, fromprofileid);
	}
	void Peer::send_authorize_add(int profileid) {
		std::ostringstream s;
		s << "\\addbuddyresponse\\" << GPI_BM_REQUEST; //the addbuddy response might be implemented wrong
		s << "\\newprofileid\\" << profileid;
		s << "\\confirmation\\d41d8cd98f00b204e9800998ecf8427e"; //temp until calculation fixed
		SendPacket((const uint8_t *)s.str().c_str(),s.str().length());

		s.str("");
		s << "\\bm\\" << GPI_BM_AUTH;
		s << "\\f\\" << profileid;
		s << "\\msg\\" << "I have authorized your request to add me to your list";
		s << "|signed|d41d8cd98f00b204e9800998ecf8427e"; //temp until calculation fixed
		SendPacket((const uint8_t *)s.str().c_str(),s.str().length());
	}
	void Peer::send_revoke_message(int from_profileid, int date_unix_timestamp) {
		std::ostringstream s;
		s << "\\bm\\" << GPI_BM_REVOKE;
		s << "\\f\\" << from_profileid;
		s << "\\msg\\I have revoked you from my list.";
		s << "|signed|d41d8cd98f00b204e9800998ecf8427e"; //temp until calculation fixed
		if(date_unix_timestamp != 0)
			s << "\\date\\" << date_unix_timestamp;
		SendPacket((const uint8_t *)s.str().c_str(),s.str().length());
	}
	void Peer::send_buddy_message(char type, int from_profileid, int timestamp, const char *msg) {
		std::ostringstream s;
		s << "\\bm\\" << ((int)type);
		s << "\\f\\" << from_profileid;
		if(timestamp != 0)
			s << "\\date\\" << timestamp;
		s << "\\msg\\" << msg;
		SendPacket((const uint8_t *)s.str().c_str(),s.str().length());
	}
	void Peer::handle_getprofile(const char *data, int len) {
		OS::ProfileSearchRequest request;
		int profileid = find_paramint("profileid",(char *)data);		
		m_search_operation_id = find_paramint("id",(char *)data);		
		request.profile_search_details.id = profileid;
		request.extra = this;
		request.callback = Peer::m_getprofile_callback;
		OS::ProfileSearchTask::getProfileTask()->AddRequest(request);
	}
	void Peer::handle_addblock(const char *data, int len) {
		int profileid = find_paramint("profileid",(char *)data);
		GPBackend::GPBackendRedisTask::MakeBlockRequest(m_profile.id, profileid);
	}
	void Peer::handle_removeblock(const char *data, int len) {
		int profileid = find_paramint("profileid",(char *)data);
		GPBackend::GPBackendRedisTask::MakeRemoveBlockRequest(m_profile.id, profileid);
	}
	void Peer::handle_keepalive(const char *data, int len) {
		std::ostringstream s;
		s << "\\ka\\";
		SendPacket((const uint8_t *)s.str().c_str(),s.str().length());
	}
	void Peer::handle_bm(const char *data, int len) {
		char msg[GP_REASON_LEN+1];
		int to_profileid = find_paramint("t", (char *)data);
		int msg_type = find_paramint("bm", (char *)data);
		if(!find_param("msg",(char *)data, (char *)&msg,GP_REASON_LEN)) {
			return;
		}

		switch(msg_type) {
			case GPI_BM_MESSAGE:
			case GPI_BM_UTM:
			case GPI_BM_PING:
			case GPI_BM_PONG:
				break;
			default:
				return;
		}

		GPBackend::GPBackendRedisTask::SendMessage(this, to_profileid, msg_type, msg);
	}
	void Peer::m_getprofile_callback(OS::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra) {
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
			s << "\\userid\\" << p.userid;

			if(p.nick.length()) {
				s << "\\nick\\" << p.nick;
			}

			if(p.uniquenick.length()) {
				s << "\\uniquenick\\" << p.uniquenick;
			}

			if(user.publicmask & GP_MASK_EMAIL || peer->m_profile.userid == user.id) {
				if(user.email.length()) {
					s << "\\email\\" << user.email;
				}
			}

			if(p.firstname.length()) {
				s << "\\firstname\\" << p.firstname;
			}

			if(p.lastname.length()) {
				s << "\\lastname\\" << p.lastname;
			}

			if(user.publicmask & GP_MASK_HOMEPAGE || peer->m_profile.userid == user.id) {
				if(p.homepage.length()) {
					s << "\\homepage\\" << p.homepage;
				}
			}

			if(user.publicmask & GP_MASK_SEX || peer->m_profile.userid == user.id) {
				s << "\\sex\\" << p.sex;
			}

			if(p.icquin) {
				s << "\\icquin\\" << p.icquin;
			}

			if(user.publicmask & GP_MASK_ZIPCODE || peer->m_profile.userid == user.id) {
				if(p.zipcode) {
					s << "\\zipcode\\" << p.zipcode;
				}
			}

			if(p.pic) {
				s << "\\pic\\" << p.pic;
			}

			if(p.ooc) {
				s << "\\ooc\\" << p.ooc;
			}

			if(p.ind) {
				s << "\\ind\\" << p.ind;
			}

			if(p.mar) {
				s << "\\pic\\" << p.mar;
			}

			if(user.publicmask & GP_MASK_BIRTHDAY || peer->m_profile.userid == user.id) {
				s << "\\birthday\\" << p.birthday;
			}

			if(user.publicmask & GP_MASK_COUNTRYCODE || peer->m_profile.userid == user.id) {
				s << "\\countrycode\\" << p.countrycode;
			}
			s << "\\aim\\" << p.aim;

			s << "\\videocard1string\\" << p.videocardstring[0];
			s << "\\videocard2string\\" << p.videocardstring[1];
			s << "\\osstring\\" << p.osstring;


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
	void Peer::perform_nick_email_auth(const char *nick_email, int partnercode, const char *server_challenge, const char *client_challenge, const char *response, int operation_id) {
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
 		OS::AuthTask::TryAuthNickEmail_GPHash(nick, email, partnercode, server_challenge, client_challenge, response, m_nick_email_auth_cb, this, operation_id);
	}
	void Peer::m_nick_email_auth_cb(bool success, OS::User user, OS::Profile profile, OS::AuthData auth_data, void *extra, int operation_id) {
		Peer *peer = (Peer *)extra;
		if(!g_gbl_gp_driver->HasPeer(peer)) {
			return;
		}
		if(!peer->m_backend_session_key.length() && auth_data.session_key)
			peer->m_backend_session_key = auth_data.session_key;

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
			ss << "\\id\\" << operation_id;

			peer->SendPacket((const uint8_t *)ss.str().c_str(),ss.str().length());

			peer->send_buddies();
			peer->send_blocks();

			peer->send_backend_auth_event();
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
	void Peer::inform_status_update(int profileid, GPStatus status, bool no_update) {
		std::ostringstream ss;
		if(!no_update)
			mp_mutex->lock();
		if(m_buddies.find(profileid) != m_buddies.end()) {

			if(!no_update)
				m_buddies[profileid] = status;

			if(std::find(m_blocked_by.begin(), m_blocked_by.end(), profileid) != m_blocked_by.end()) {
				status = GPShared::gp_default_status;
			}

			ss << "\\bm\\" << GPI_BM_STATUS;

			ss << "\\f\\" << profileid;

			ss << "\\msg\\";

			ss << "|s|" << status.status;
			if(status.status_str[0] != 0)
				ss << "|ss|" << status.status_str;

			if(status.location_str[0] != 0)
				ss << "|ls|" << status.location_str;

			if(status.address.ip != 0) {
				ss << "|ip|" << status.address.ip;
				ss << "|p|" << status.address.port;
			}

			if(status.quiet_flags != GP_SILENCE_NONE) {
				ss << "|qm|" << status.quiet_flags;
			}
			SendPacket((const uint8_t *)ss.str().c_str(),ss.str().length());
		}
		if(!no_update)
			mp_mutex->unlock();
	}
	void Peer::send_backend_auth_event() {
		GPBackend::GPBackendRedisTask::SendLoginEvent(this);
	}
	void Peer::send_error(GPErrorCode code) {
		GPShared::GPErrorData error_data = GPShared::getErrorDataByCode(code);
		if(error_data.msg == NULL) {
			m_delete_flag = true;
			return;
		}
		std::ostringstream ss;
		ss << "\\error\\";
		ss << "\\err\\" << error_data.error;
		ss << "\\errmsg\\" << error_data.msg;
		if(error_data.die) {
			ss << "\\fatal\\" << error_data.die;
			m_delete_flag = true;
		}
		SendPacket((const uint8_t *)ss.str().c_str(),ss.str().length());
	}
	void Peer::send_user_blocked(int from_profileid) {
		if(std::find(m_blocked_by.begin(), m_blocked_by.end(), from_profileid) == m_blocked_by.end()) {
			m_blocked_by.push_back(from_profileid);
			if(m_buddies.find(from_profileid) != m_buddies.end()) {
				inform_status_update(from_profileid, m_buddies[from_profileid], true);
			}
		}
	}
	void Peer::send_user_block_deleted(int from_profileid) {
		if(std::find(m_blocked_by.begin(), m_blocked_by.end(), from_profileid) != m_blocked_by.end()) {
			std::vector<int>::iterator it = m_blocked_by.begin();
			while(it != m_blocked_by.end()) {
				int pid = *it;
				if(pid == from_profileid) {
					m_blocked_by.erase(it);

					if(m_buddies.find(pid) != m_buddies.end()) {
						inform_status_update(pid, m_buddies[pid], true);
					}
					return;
				}
				it++;
			}
		}
	}
	int Peer::GetProfileID() {
		return m_profile.id;
	}
}