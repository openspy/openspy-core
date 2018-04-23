#include "GPPeer.h"
#include "GPDriver.h"
#include <OS/OpenSpy.h>
#include <OS/Search/Profile.h>
#include <OS/legacy/helpers.h>

#include <OS/Search/User.h>

#include <OS/Buffer.h>
#include <OS/KVReader.h>
#include <sstream>
#include <algorithm>

#include "GPBackend.h"

using namespace GPShared;

#define LoadParamInt(read_name, write_var, var_name) if(var_name.HasKey(read_name)) { \
		write_var = var_name.GetValueInt(read_name); \
	} else { \
		write_var = 0;\
	}

#define LoadParam(read_name, write_var, var_name) if(var_name.HasKey(read_name)) { \
		write_var = var_name.GetValue(read_name); \
	}

namespace GP {

	Peer::Peer(Driver *driver, INetIOSocket *sd) : INetPeer(driver, sd) {
		ResetMetrics();
		m_delete_flag = false;
		m_timeout_flag = false;
		mp_mutex = OS::CreateMutex();
		gettimeofday(&m_last_ping, NULL);
		gettimeofday(&m_last_recv, NULL);
		gettimeofday(&m_status_refresh, NULL);

		m_status.status = GP_OFFLINE;
		m_status.status_str[0] = 0;
		m_status.location_str[0] = 0;
		m_status.quiet_flags = GP_SILENCE_NONE;
		m_status.address = sd->address;

		m_game.gameid = 0;
		m_game.compatibility_flags = 0;

		OS::LogText(OS::ELogLevel_Info, "[%s] New connection", m_sd->address.ToString().c_str());

		gen_random(m_challenge, CHALLENGE_LEN);

		send_login_challenge(1);
	}
	Peer::~Peer() {
		OS::LogText(OS::ELogLevel_Info, "[%s] Connection closed, timeout: %d", m_sd->address.ToString().c_str(), m_timeout_flag);
		delete mp_mutex;
	}
	void Peer::Delete(bool timeout) {
		GPBackend::GPBackendRedisRequest req;
		req.type = GPBackend::EGPRedisRequestType_UpdateStatus;
		req.peer = this;
		req.peer->IncRef();
		req.StatusInfo = GPShared::gp_default_status;
		req.extra = (void *)req.peer;
		GPBackend::m_task_pool->AddRequest(req);

		m_delete_flag = true;
		m_timeout_flag = timeout;
	}
	void Peer::think(bool packet_waiting) {
		
		NetIOCommResp io_resp;
		if (m_delete_flag) return;

		if (packet_waiting) {
			io_resp = this->GetDriver()->getServer()->getNetIOInterface()->streamRecv(m_sd, m_recv_buffer);

			int len = m_recv_buffer.size();
			if (io_resp.disconnect_flag || io_resp.error_flag || len == 0) {
				goto end;
			}

			m_peer_stats.packets_in++;
			m_peer_stats.bytes_in += len;

			/*
			This scans the incoming packets for \\final\\ and splits based on that,


			as well as handles incomplete packets -- due to TCP preserving data order, this is possible, and cannot be used on UDP protocols
			*/
			std::string recv_buf = m_kv_accumulator;
			m_kv_accumulator.clear();
			recv_buf.append((const char *)m_recv_buffer.GetHead(), len);

			size_t final_pos = 0, last_pos = 0;

			do {
				final_pos = recv_buf.find("\\final\\", last_pos);
				if (final_pos == std::string::npos) break;

				std::string partial_string = recv_buf.substr(last_pos, final_pos - last_pos);
				handle_packet(partial_string);

				last_pos = final_pos + 7; // 7 = strlen of \\final
			} while (final_pos != std::string::npos);


			//check for extra data that didn't have the final string -- incase of incomplete data
			if (last_pos < len) {
				std::string remaining_str = recv_buf.substr(last_pos);
				m_kv_accumulator.append(remaining_str);
			}

			if (m_kv_accumulator.length() > MAX_UNPROCESSED_DATA) {
				Delete();
				return;
			}
		}

	end:
		send_ping();

		//check for timeout
		struct timeval current_time;
		gettimeofday(&current_time, NULL);
		if (current_time.tv_sec - m_last_recv.tv_sec > GP_PING_TIME * 2) {
			Delete(true);
		} else if ((io_resp.disconnect_flag || io_resp.error_flag) && packet_waiting) {
			Delete();
		}
	}
	void Peer::handle_packet(std::string packet) {
		OS::KVReader data_parser = OS::KVReader(packet);
		gettimeofday(&m_last_recv, NULL);

		std::string command = data_parser.GetKeyByIdx(0);

		if(!command.compare("login")) {
			handle_login(data_parser);
			goto end;
		} else if(!command.compare("ka")) {
			handle_keepalive(data_parser);
			goto end;
		} else if (command.compare("logout") == 0) {
			Delete();
			return;
		}
		else if (command.compare("newuser") == 0) {
			handle_newuser(data_parser);
			goto end;
		}
		if(m_backend_session_key.length() > 0) {
			if(command.compare("status") == 0) {
				handle_status(data_parser);
			} else if(command.compare("addbuddy") == 0) {
				handle_addbuddy(data_parser);
			} else if(command.compare("delbuddy") == 0) {
				handle_delbuddy(data_parser);
			} else if(command.compare("addblock") == 0) {
				handle_addblock(data_parser);
			} else if(command.compare("removeblock") == 0) {
				handle_removeblock(data_parser);
			} else if(command.compare("revoke") == 0) {
				handle_revoke(data_parser);
			} else if(command.compare("authadd") == 0) {
				handle_authadd(data_parser);
			} else if(command.compare("getprofile") == 0) {
				handle_getprofile(data_parser);
			} else if(command.compare("bm") == 0) {
				handle_bm(data_parser);
			} else if(command.compare("pinvite") == 0) {
				handle_pinvite(data_parser);
			} else if(command.compare("newprofile") == 0) {
				handle_newprofile(data_parser);
			} else if(command.compare("delprofile") == 0) {
				handle_delprofile(data_parser);
			} else if(command.compare("registernick") == 0) {
				handle_registernick(data_parser);
			} else if(command.compare("registercdkey") == 0) {
				handle_registercdkey(data_parser);
			} else if(command.compare("updatepro") == 0) {
				handle_updatepro(data_parser);
			}
		}
		end:
		struct timeval current_time;
		gettimeofday(&current_time, NULL);
		if (current_time.tv_sec - m_status_refresh.tv_sec > (GP_STATUS_EXPIRE_TIME / 2)) {
			gettimeofday(&m_status_refresh, NULL);
			//often called with keep alives
			if (m_profile.id)
				GPBackend::GPBackendRedisTask::SetPresenceStatus(m_profile.id, m_status, this);
		}
	}
	void Peer::handle_newuser(OS::KVReader data_parser) {
		std::string nick;
		std::string uniquenick;
		std::string email;
		std::string passenc;
		std::string password;
		std::string gamename;
		int partnercode = data_parser.GetValueInt("partnerid");
		int namespaceid = data_parser.GetValueInt("namespaceid");
		if (data_parser.HasKey("email")) {
			email = data_parser.GetValue("email");
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
			int passlen = passenc.length();
			char *dpass = (char *)base64_decode((uint8_t *)passenc.c_str(), &passlen);
			passlen = gspassenc((uint8_t *)dpass);
			password = dpass;
			if (dpass)
				free((void *)dpass);
		}
		else if (data_parser.HasKey("passwordenc")) {
			passenc = data_parser.GetValue("passwordenc");
			int passlen = passenc.length();
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
			return;
		}
				
		int id = data_parser.GetValueInt("id");

		OS::AuthTask::TryCreateUser_OrProfile(nick, uniquenick, namespaceid, email, partnercode, password, false, m_newuser_cb, this, id, this, gamename);
	}
	void Peer::m_newuser_cb(bool success, OS::User user, OS::Profile profile, OS::AuthData auth_data, void *extra, int operation_id, INetPeer *peer) {
		int err_code = 0;
		std::ostringstream s;
		if (!success) {
			err_code = (int)GP_NEWUSER_BAD_NICK;
			switch (auth_data.response_code) {
				//case OS::CREATE_RESPONE_NICK_IN_USE:
				case OS::CREATE_RESPONE_UNIQUENICK_IN_USE:
					err_code = GP_NEWUSER_BAD_NICK;
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
				case OS::LOGIN_RESPONSE_SERVER_ERROR:
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

		if (auth_data.gamedata.gameid != 0) {
			((Peer *)peer)->m_game = auth_data.gamedata;
		}

		if (((Peer *)peer)->m_game.compatibility_flags & OS_COMPATIBILITY_FLAG_GP_DISCONNECT_ON_NEWUSER) {
			((Peer *)peer)->m_delete_flag = true;
		}
	
	}
	void Peer::m_update_profile_callback(OS::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer) {
	}
	/*
		Updates profile specific information
	*/
	void Peer::handle_updatepro(OS::KVReader data_parser) {
		bool send_userupdate = false;

		//OS::KVReader data_parser = OS::KVReader(std::string(data));
		OS::Profile profile = m_profile;
		OS::User user = m_user;

		LoadParamInt("zipcode", profile.zipcode, data_parser)
		LoadParamInt("sex", profile.sex, data_parser)
		LoadParamInt("pic", profile.pic, data_parser)
		LoadParamInt("ooc", profile.ooc, data_parser)
		LoadParamInt("ind", profile.ind, data_parser)
		LoadParamInt("mar", profile.mar, data_parser)
		LoadParamInt("chc", profile.chc, data_parser)
		LoadParamInt("i1", profile.i1, data_parser)
		LoadParamInt("publicmask", user.publicmask, data_parser) //user param

		if (data_parser.HasKey("birthday")) {
			profile.birthday = OS::Date::GetDateFromGPValue(data_parser.GetValueInt("birthday"));

		}

		if (data_parser.HasKey("publicmask")) {
				send_userupdate = true;
		}
		LoadParam("nick", profile.nick, data_parser)
		LoadParam("uniquenick", profile.uniquenick, data_parser)
		LoadParam("firstname", profile.firstname, data_parser)
		LoadParam("lastname", profile.lastname, data_parser)
		LoadParam("countrycode", profile.countrycode, data_parser)
		LoadParam("videocard1string", profile.videocardstring[0], data_parser)
		LoadParam("videocard2string", profile.videocardstring[1], data_parser)
		LoadParam("osstring", profile.osstring, data_parser)
		LoadParam("aim", profile.aim, data_parser)

		OS::ProfileSearchRequest request;
		request.profile_search_details = profile;
		request.extra = this;
		request.peer = this;
		request.peer->IncRef();
		request.type = OS::EProfileSearch_UpdateProfile;
		request.callback = Peer::m_update_profile_callback;
		OS::m_profile_search_task_pool->AddRequest(request);

		if(send_userupdate) {
			OS::UserSearchRequest user_request;
			user_request.search_params = user;
			user_request.type = OS::EUserRequestType_Update;
			user_request.extra = this;
			user_request.peer = this;
			user_request.peer->IncRef();
			user_request.callback = NULL;
			OS::m_user_search_task_pool->AddRequest(user_request);
		}
	}
	void Peer::handle_updateui(OS::KVReader data_parser) {
		char buff[GP_STATUS_STRING_LEN + 1];
		OS::UserSearchRequest request;

		//OS::KVReader data_parser = OS::KVReader(std::string(data));
		
		LoadParamInt("cpubrandid", m_user.cpu_brandid, data_parser)
		LoadParamInt("cpuspeed", m_user.cpu_speed, data_parser)
		LoadParamInt("vidocard1ram", m_user.videocard_ram[0], data_parser)
		LoadParamInt("vidocard2ram", m_user.videocard_ram[1], data_parser)
		LoadParamInt("connectionid", m_user.connectionid, data_parser)
		LoadParamInt("connectionspeed", m_user.connectionspeed, data_parser)
		LoadParamInt("hasnetwork", m_user.hasnetwork, data_parser)
		LoadParam("email", m_user.email, data_parser)


		if(data_parser.HasKey("passwordenc")) {
			std::string pwenc = data_parser.GetValue("passwordenc");
			int passlen = pwenc.length();
			char *dpass = (char *)base64_decode((uint8_t *)pwenc.c_str(), &passlen);
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
		OS::m_user_search_task_pool->AddRequest(request);
	}
	void Peer::handle_registernick(OS::KVReader data_parser) {
		
	}
	void Peer::handle_registercdkey(OS::KVReader data_parser) {

	}
	void Peer::m_create_profile_callback(OS::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer) {
		if(response_reason == OS::EProfileResponseType_Success && results.size() > 0) {
			OS::Profile profile = results.front();
		}
	}
	void Peer::handle_newprofile(OS::KVReader data_parser) {
		OS::ProfileSearchRequest request;
		int replace = data_parser.GetValueInt("replace");
		std::string nick, oldnick;
		nick = data_parser.GetValue("nick");
		
		if(replace) { //TODO: figure out replaces functionality
			oldnick = data_parser.GetValue("oldnick");
		}
		request.profile_search_details.id = m_profile.id;
		request.profile_search_details.nick = nick;
		request.extra = this;
		request.peer = this;
		request.peer->IncRef();
		request.type = OS::EProfileSearch_CreateProfile;
		request.callback = Peer::m_create_profile_callback;
		OS::m_profile_search_task_pool->AddRequest(request);
	}
	void Peer::m_delete_profile_callback(OS::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer) {
		std::ostringstream s;
		
		s << "\\dpr\\" << (int)(response_reason == OS::EProfileResponseType_Success);
		((GP::Peer *)peer)->SendPacket((const uint8_t *)s.str().c_str(),s.str().length());
	}
	void Peer::handle_delprofile(OS::KVReader data_parser) {
		OS::ProfileSearchRequest request;
		request.profile_search_details.id = m_profile.id;
		request.extra = this;
		request.peer = this;
		request.peer->IncRef();
		request.type = OS::EProfileSearch_DeleteProfile;
		request.callback = Peer::m_delete_profile_callback;
		OS::m_profile_search_task_pool->AddRequest(request);
	}
	void Peer::handle_login(OS::KVReader data_parser) {
		char gamename[33 + 1];
		
		int partnercode = data_parser.GetValueInt("partnerid");
		int peer_port = data_parser.GetValueInt("port");
		int sdkrev = data_parser.GetValueInt("sdkrevision");
		int namespaceid = data_parser.GetValueInt("namespaceid");
		int quiet = data_parser.GetValueInt("quiet");

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
	void Peer::handle_pinvite(OS::KVReader data_parser) {
		//profileid\10000\productid\1
		std::ostringstream s;
		//OS::KVReader data_parser = OS::KVReader(std::string(data));
		int profileid = 0;
		if (data_parser.HasKey("profileid")) {
			profileid = data_parser.GetValueInt("profileid");
		}
		int productid = 0;
		if (data_parser.HasKey("productid")) {
			productid = data_parser.GetValueInt("productid");
		}

		s << "|p|" << productid;
		s << "|l|" << m_status.location_str;
		s << "|signed|d41d8cd98f00b204e9800998ecf8427e"; //temp until calculation fixed
		GPBackend::GPBackendRedisTask::SendMessage(this, profileid, GPI_BM_INVITE, s.str().c_str());
	}
	void Peer::handle_status(OS::KVReader data_parser) {
		//OS::KVReader data_parser = OS::KVReader(std::string(data));
		if (data_parser.HasKey("status")) {
			m_status.status = (GPEnum)data_parser.GetValueInt("status");
		}
		if (data_parser.HasKey("statstring")) {
			m_status.status_str = data_parser.GetValue("statstring");
		}
		if (data_parser.HasKey("locstring")) {
			m_status.location_str = data_parser.GetValue("locstring");
		}

		GPBackend::GPBackendRedisTask::SetPresenceStatus(m_profile.id, m_status, this);
	}
	void Peer::handle_statusinfo(OS::KVReader data_parser) {

	}
	void Peer::handle_addbuddy(OS::KVReader data_parser) {
		//OS::KVReader data_parser = OS::KVReader(std::string(data));
		int newprofileid = 0;
		std::string reason;
		if (data_parser.HasKey("reason")) {
			reason = data_parser.GetValue("reason");
		}

		if (data_parser.HasKey("newprofileid")) {
			newprofileid = data_parser.GetValueInt("newprofileid");
		}
		else {
			send_error(GPShared::GP_PARSE);
			return;
		}

		mp_mutex->lock();
		if(m_buddies.find(newprofileid) != m_buddies.end()) {
			mp_mutex->unlock();
			send_error(GP_ADDBUDDY_ALREADY_BUDDY);
			return;
		} else if(std::find(m_blocks.begin(),m_blocks.end(), newprofileid) != m_blocks.end()) {
			mp_mutex->unlock();
			send_error(GP_ADDBUDDY_IS_ON_BLOCKLIST);
			return;
		}
		mp_mutex->unlock();

		GPBackend::GPBackendRedisTask::MakeBuddyRequest(this, newprofileid, reason);
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
	void Peer::m_buddy_list_lookup_callback(OS::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer) {
		std::ostringstream s;
		std::string str;
		std::vector<OS::Profile>::iterator it = results.begin();
		((GP::Peer *)peer)->mp_mutex->lock();
		if(results.size() > 0) {
			s << "\\bdy\\" << results.size();
			s << "\\list\\";
			while(it != results.end()) {
				OS::Profile p = *it;
				s << p.id << ",";

				if(((GP::Peer *)peer)->m_buddies.find(p.id) == ((GP::Peer *)peer)->m_buddies.end())
					((GP::Peer *)peer)->m_buddies[p.id] = GPShared::gp_default_status;

				it++;
			}
			str = s.str();
			str = str.substr(0, str.size()-1);
			((GP::Peer *)peer)->SendPacket((const uint8_t *)str.c_str(),str.length());

			((GP::Peer *)peer)->refresh_buddy_list();
		}
		((GP::Peer *)peer)->mp_mutex->unlock();
	}
	void Peer::m_block_list_lookup_callback(OS::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer) {
		std::ostringstream s;
		std::string str;
		std::vector<OS::Profile>::iterator it = results.begin();
		((GP::Peer *)peer)->mp_mutex->lock();
		if(results.size() > 0) {
			s << "\\blk\\" << results.size();
			s << "\\list\\";
			while(it != results.end()) {
				OS::Profile p = *it;
				s << p.id << ",";
				((GP::Peer *)peer)->m_blocks.push_back(p.id);
				it++;
			}
			str = s.str();
			str = str.substr(0, str.size()-1);
			((GP::Peer *)peer)->SendPacket((const uint8_t *)str.c_str(),str.length());

			GPBackend::GPBackendRedisRequest req;
			req.extra = (void *)peer;
			req.peer = (GP::Peer *)peer;
			peer->IncRef();
			req.type = GPBackend::EGPRedisRequestType_SendGPBlockStatus;
			GPBackend::m_task_pool->AddRequest(req);

		}
		((GP::Peer *)peer)->mp_mutex->unlock();
	}
	void Peer::send_buddies() {
		OS::ProfileSearchRequest request;
		request.profile_search_details.id = m_profile.id;
		request.extra = this;
		request.peer = this;
		request.peer->IncRef();
		request.type = OS::EProfileSearch_Buddies;
		request.callback = Peer::m_buddy_list_lookup_callback;
		OS::m_profile_search_task_pool->AddRequest(request);
	}
	void Peer::send_blocks() {
		OS::ProfileSearchRequest request;
		request.profile_search_details.id = m_profile.id;
		request.extra = this;
		request.peer = this;
		request.peer->IncRef();
		request.type = OS::EProfileSearch_Blocks;
		request.callback = Peer::m_block_list_lookup_callback;
		OS::m_profile_search_task_pool->AddRequest(request);
	}
	void Peer::handle_delbuddy(OS::KVReader data_parser) {
		if (data_parser.HasKey("delprofileid")) {
			int delprofileid = data_parser.GetValueInt("delprofileid");
			if (m_buddies.find(delprofileid) != m_buddies.end()) {
				GPBackend::GPBackendRedisTask::MakeDelBuddyRequest(this, delprofileid);
				m_buddies.erase(delprofileid);
			}
		}
		else {
			send_error(GPShared::GP_PARSE);
			return;
		}
	}
	void Peer::handle_revoke(OS::KVReader data_parser) {
		if (data_parser.HasKey("profileid")) {
			int delprofileid = data_parser.GetValueInt("profileid");
			GPBackend::GPBackendRedisTask::MakeRevokeAuthRequest(this, delprofileid);
		} else {
			send_error(GPShared::GP_PARSE);
			return;
		}

	}
	void Peer::handle_authadd(OS::KVReader data_parser) {
		if (data_parser.HasKey("fromprofileid")) {
			int fromprofileid = data_parser.GetValueInt("fromprofileid");
			GPBackend::GPBackendRedisTask::MakeAuthorizeBuddyRequest(this, fromprofileid);
		} else {
			send_error(GPShared::GP_PARSE);
			return;
		}
		
		
	}
	void Peer::refresh_buddy_list() {
		GPBackend::GPBackendRedisRequest req;
		req.extra = (void *)this;
		req.peer = (GP::Peer *)this;
		IncRef();
		req.type = GPBackend::EGPRedisRequestType_SendGPBuddyStatus;
		GPBackend::m_task_pool->AddRequest(req);
	}
	void Peer::send_authorize_add(int profileid, bool silent) {
		if (!silent) {
			std::ostringstream s;
			s << "\\addbuddyresponse\\" << GPI_BM_REQUEST; //the addbuddy response might be implemented wrong
			s << "\\newprofileid\\" << profileid;
			s << "\\confirmation\\d41d8cd98f00b204e9800998ecf8427e"; //temp until calculation fixed
			SendPacket((const uint8_t *)s.str().c_str(), s.str().length());

			s.str("");
			s << "\\bm\\" << GPI_BM_AUTH;
			s << "\\f\\" << profileid;
			s << "\\msg\\" << "I have authorized your request to add me to your list";
			s << "|signed|d41d8cd98f00b204e9800998ecf8427e"; //temp until calculation fixed
			SendPacket((const uint8_t *)s.str().c_str(), s.str().length());
		}

		m_buddies[profileid] = GPShared::gp_default_status;

		refresh_buddy_list();


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

		refresh_buddy_list();
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
	void Peer::handle_getprofile(OS::KVReader data_parser) {
		OS::ProfileSearchRequest request;
		//OS::KVReader data_parser = OS::KVReader(std::string(data));
		if (data_parser.HasKey("profileid") && data_parser.HasKey("id")) {
			int profileid = data_parser.GetValueInt("profileid");
			m_search_operation_id = data_parser.GetValueInt("id");
			request.profile_search_details.id = profileid;
			request.extra = this;
			request.peer = this;
			request.peer->IncRef();
			request.callback = Peer::m_getprofile_callback;
			request.type = OS::EProfileSearch_Profiles;
			OS::m_profile_search_task_pool->AddRequest(request);
		}
	}
	void Peer::handle_addblock(OS::KVReader data_parser) {
		if (data_parser.HasKey("profileid")) {
			int profileid = data_parser.GetValueInt("profileid");
			GPBackend::GPBackendRedisTask::MakeBlockRequest(this, profileid);
		} else {
			send_error(GPShared::GP_PARSE);
			return;
		}
		
	}
	void Peer::handle_removeblock(OS::KVReader data_parser) {
		//OS::KVReader data_parser = OS::KVReader(std::string(data));
		if (data_parser.HasKey("profileid")) {
			int profileid = data_parser.GetValueInt("profileid");
			GPBackend::GPBackendRedisTask::MakeRemoveBlockRequest(this, profileid);
		}  else {
			send_error(GPShared::GP_PARSE);
			return;
		}
	}
	void Peer::handle_keepalive(OS::KVReader data_parser) {
		//std::ostringstream s;
		//s << "\\ka\\";
		//SendPacket((const uint8_t *)s.str().c_str(),s.str().length());
	}
	void Peer::handle_bm(OS::KVReader data_parser) {
		char msg[GP_REASON_LEN+1];


		//OS::KVReader data_parser = OS::KVReader(std::string(data));
		if (data_parser.HasKey("t") && data_parser.HasKey("bm") && data_parser.HasKey("msg")) {
			int to_profileid = data_parser.GetValueInt("t");
			int msg_type = data_parser.GetValueInt("bm");
			std::string msg = data_parser.GetValue("msg");
			
			switch (msg_type) {
			case GPI_BM_MESSAGE:
			case GPI_BM_UTM:
			case GPI_BM_PING:
			case GPI_BM_PONG:
				break;
			default:
				return;
			}

			GPBackend::GPBackendRedisTask::SendMessage(this, to_profileid, msg_type, msg.c_str());
		}
		else {
			send_error(GPShared::GP_PARSE);
		}
	}
	void Peer::m_getprofile_callback(OS::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer) {
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
			if(user.publicmask & GP_MASK_EMAIL || ((GP::Peer *)peer)->m_profile.userid == user.id) {
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

			if(user.publicmask & GP_MASK_HOMEPAGE || ((GP::Peer *)peer)->m_profile.userid == user.id) {
				if(p.homepage.length()) {
					s << "\\homepage\\" << p.homepage;
				}
			}

			if(user.publicmask & GP_MASK_SEX || ((GP::Peer *)peer)->m_profile.userid == user.id) {
				s << "\\sex\\" << p.sex;
			}

			if(p.icquin) {
				s << "\\icquin\\" << p.icquin;
			}

			if(user.publicmask & GP_MASK_ZIPCODE || ((GP::Peer *)peer)->m_profile.userid == user.id) {
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

			if(user.publicmask & GP_MASK_BIRTHDAY || ((GP::Peer *)peer)->m_profile.userid == user.id) {
				s << "\\birthday\\" << p.birthday.GetGPDate();
			}

			if(user.publicmask & GP_MASK_COUNTRYCODE || ((GP::Peer *)peer)->m_profile.userid == user.id) {
				s << "\\countrycode\\" << p.countrycode;
			}
			s << "\\aim\\" << p.aim;

			s << "\\videocard1string\\" << p.videocardstring[0];
			s << "\\videocard2string\\" << p.videocardstring[1];
			s << "\\osstring\\" << p.osstring;


			s << "\\id\\" << ((GP::Peer *)peer)->m_search_operation_id;

			s << "\\sig\\d41d8cd98f00b204e9800998ecf8427e"; //temp until calculation fixed

			((GP::Peer *)peer)->SendPacket((const uint8_t *)s.str().c_str(),s.str().length());
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
		OS::Buffer buffer;
		buffer.WriteBuffer((void *)buff, len);
		if(attach_final) {
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
	void Peer::send_ping() {
		//check for timeout
		
		struct timeval current_time;
		gettimeofday(&current_time, NULL);
		if(current_time.tv_sec - m_last_ping.tv_sec > GP_PING_TIME) {
			gettimeofday(&m_last_ping, NULL);
			//std::string ping_packet = "\\ka\\";
			//SendPacket((const uint8_t *)ping_packet.c_str(),ping_packet.length());
		}
	}
	void Peer::perform_uniquenick_auth(const char *uniquenick, int partnercode, int namespaceid, const char *server_challenge, const char *client_challenge, const char *response, int operation_id, INetPeer *peer) {
		OS::AuthTask::TryAuthUniqueNick_GPHash(uniquenick, partnercode, namespaceid, server_challenge, client_challenge, response, m_auth_cb, this, operation_id, peer);
	}
	void Peer::perform_nick_email_auth(const char *nick_email, int partnercode, int namespaceid, const char *server_challenge, const char *client_challenge, const char *response, int operation_id, INetPeer *peer) {
		const char *email = NULL;
		char nick[31 + 1];
		const char *first_at = strchr(nick_email, '@');
		if(first_at) {
			unsigned int nick_len = first_at - nick_email;
			if(nick_len < sizeof(nick)) {
				strncpy(nick, nick_email, nick_len);
				nick[nick_len] = 0;
			}
			email = first_at+1;
		}
 		OS::AuthTask::TryAuthNickEmail_GPHash(nick, email, partnercode, namespaceid, server_challenge, client_challenge, response, m_auth_cb, this, operation_id, this);
	}
	void Peer::perform_preauth_auth(const char *auth_token, const char *server_challenge, const char *client_challenge, const char *response, int operation_id, INetPeer *peer) {
		OS::AuthTask::TryAuthTicket(auth_token, server_challenge, client_challenge, response, m_auth_cb, operation_id, this);
	}
	void Peer::m_auth_cb(bool success, OS::User user, OS::Profile profile, OS::AuthData auth_data, void *extra, int operation_id, INetPeer *peer) {
		if(!((GP::Peer *)peer)->m_backend_session_key.length() && auth_data.session_key.length())
			((GP::Peer *)peer)->m_backend_session_key = auth_data.session_key;

		((GP::Peer *)peer)->m_user = user;
		((GP::Peer *)peer)->m_profile = profile;

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
			if(auth_data.hash_proof.length()) {
				ss << "\\proof\\" << auth_data.hash_proof;
			}
			ss << "\\id\\" << operation_id;

			((GP::Peer *)peer)->SendPacket((const uint8_t *)ss.str().c_str(),ss.str().length());

			((GP::Peer *)peer)->send_buddies();
			((GP::Peer *)peer)->send_blocks();

			((GP::Peer *)peer)->send_backend_auth_event();
		} else {
			switch(auth_data.response_code) {
				case OS::LOGIN_RESPONSE_USER_NOT_FOUND:
					((GP::Peer *)peer)->send_error(GP_LOGIN_BAD_EMAIL);
				break;
				case OS::LOGIN_RESPONSE_INVALID_PASSWORD:
					((GP::Peer *)peer)->send_error(GP_LOGIN_BAD_PASSWORD);
				break;
				case OS::LOGIN_RESPONSE_INVALID_PROFILE:
					((GP::Peer *)peer)->send_error(GP_LOGIN_BAD_PROFILE);
				break;
				case OS::LOGIN_RESPONSE_UNIQUE_NICK_EXPIRED:
					((GP::Peer *)peer)->send_error(GP_LOGIN_BAD_UNIQUENICK);
				break;
				case OS::LOGIN_RESPONSE_DB_ERROR:
					((GP::Peer *)peer)->send_error(GP_DATABASE);
				break;
				case OS::LOGIN_RESPONSE_SERVERINITFAILED:
				case OS::LOGIN_RESPONSE_SERVER_ERROR:
					((GP::Peer *)peer)->send_error(GP_NETWORK);
				break;
			}
		}
	}
	void Peer::inform_status_update(int profileid, GPStatus status, bool no_update) {
		std::ostringstream ss;
		mp_mutex->lock();
		bool is_blocked = std::find(m_blocks.begin() ,m_blocks.end(), profileid) != m_blocks.end();
		if(m_buddies.find(profileid) != m_buddies.end() || is_blocked) {

			if(!no_update)
				m_buddies[profileid] = status;

			if (is_blocked) {
				m_blocks.push_back(profileid);
			}

			if(status.status == GPShared::GP_OFFLINE || std::find(m_blocked_by.begin(), m_blocked_by.end(), profileid) != m_blocked_by.end()) {
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
				ss << "|p|" << status.address.GetPort();
			}

			if(status.quiet_flags != GP_SILENCE_NONE) {
				ss << "|qm|" << status.quiet_flags;
			}
			SendPacket((const uint8_t *)ss.str().c_str(),ss.str().length());
		}
		mp_mutex->unlock();
	}
	void Peer::send_backend_auth_event() {

		GPBackend::GPBackendRedisTask::SendLoginEvent(this);
	}
	void Peer::send_error(GPErrorCode code, std::string addon_data) {
		GPShared::GPErrorData error_data = GPShared::getErrorDataByCode(code);
		if(error_data.msg == NULL) {
			Delete();
			return;
		}
		std::ostringstream ss;
		ss << "\\error\\";
		ss << "\\err\\" << error_data.error;
		if (error_data.die) {
			ss << "\\fatal\\";
			Delete();
		}
		ss << "\\errmsg\\" << error_data.msg;
		if(addon_data.length())
			ss << addon_data;
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
}