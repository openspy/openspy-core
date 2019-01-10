#include "SMPeer.h"
#include "SMDriver.h"
#include "SMServer.h"
#include <OS/OpenSpy.h>
#include <OS/Buffer.h>
#include <OS/Search/Profile.h>
#include <OS/Search/User.h>
#include <OS/Search/Profile.h>
#include <OS/Auth.h>

#include <OS/gamespy/gamespy.h>
#include <OS/gamespy/gsmsalg.h>

#include <sstream>

#include <OS/Task/TaskScheduler.h>
#include <OS/SharedTasks/tasks.h>


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
		gettimeofday(&m_last_ping, NULL);
		gettimeofday(&m_last_recv, NULL);
		OS::LogText(OS::ELogLevel_Info, "[%s] New connection", m_sd->address.ToString().c_str());
	}
	Peer::~Peer() {
		OS::LogText(OS::ELogLevel_Info, "[%s] Connection closed, timeout: %d", m_sd->address.ToString().c_str(), m_timeout_flag);
		delete mp_mutex;
	}
	void Peer::think(bool packet_waiting) {
		NetIOCommResp io_resp;
		if (m_delete_flag) return;

		if (packet_waiting) {
			OS::Buffer recv_buffer;
			io_resp = this->GetDriver()->getServer()->getNetIOInterface()->streamRecv(m_sd, recv_buffer);

			int len = io_resp.comm_len;

			if (len <= 0) {
				goto end;
			}

			/*
			This scans the incoming packets for \\final\\ and splits based on that,


			as well as handles incomplete packets -- due to TCP preserving data order, this is possible, and cannot be used on UDP protocols
			*/
			std::string recv_buf = m_kv_accumulator;
			m_kv_accumulator.clear();
			recv_buf.append((const char *)recv_buffer.GetHead(), len);

			size_t final_pos = 0, last_pos = 0;

			do {
				final_pos = recv_buf.find("\\final\\", last_pos);
				if (final_pos == std::string::npos) break;

				std::string partial_string = recv_buf.substr(last_pos, final_pos - last_pos);
				handle_packet(partial_string);

				last_pos = final_pos + 7; // 7 = strlen of \\final
			} while (final_pos != std::string::npos);


			//check for extra data that didn't have the final string -- incase of incomplete data
			if (last_pos < (size_t)len) {
				std::string remaining_str = recv_buf.substr(last_pos);
				m_kv_accumulator.append(remaining_str);
			}

			if (m_kv_accumulator.length() > MAX_UNPROCESSED_DATA) {
				Delete();
				return;
			}
		}

	end:
		//send_ping();

		//check for timeout
		struct timeval current_time;
		gettimeofday(&current_time, NULL);
		if (current_time.tv_sec - m_last_recv.tv_sec > SM_PING_TIME * 2) {
			Delete(true);
		} else if ((io_resp.disconnect_flag || io_resp.error_flag) && packet_waiting) {
			Delete();
		}
	}
	void Peer::handle_packet(std::string data) {
		OS::LogText(OS::ELogLevel_Debug, "[%s] Recv: %s\n", getAddress().ToString().c_str(), data.c_str());

		OS::KVReader data_parser = OS::KVReader(data);
		std::string command = data_parser.GetKeyByIdx(0);
		if(!command.compare("search")) {
			handle_search(data_parser);
		} else if(!command.compare("others")) {
			handle_others(data_parser);
		} else if(!command.compare("otherslist")) {
			handle_otherslist(data_parser);
		} else if(!command.compare("valid")) {
			handle_valid(data_parser);
		} else if(!command.compare("nicks")) {
			handle_nicks(data_parser);
		} else if(!command.compare("pmatch")) {

		} else if(!command.compare("check")) {
			handle_check(data_parser);
		} else if(!command.compare("newuser")) {
			handle_newuser(data_parser);
		} else if(!command.compare("uniquesearch") || !command.compare("searchunique")) {
			handle_searchunique(data_parser);
		} else if(!command.compare("profilelist")) {
			
		}

		/*
		[127.0.0.1:50805] Recv: \searchunique\\sesskey\0\profileid\0\uniquenick\gptestc1\namespaces\1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16\gamename\gmtest
		*/

		gettimeofday(&m_last_recv, NULL);
	}
	void Peer::m_newuser_cb(bool success, OS::User user, OS::Profile profile, TaskShared::UserRegisterData auth_data, void *extra, INetPeer *peer) {
		int err_code = 0;

		switch(auth_data.user_response_code) {
			case TaskShared::EUserResponseType_Profile_UniqueNickInUse:
				err_code = GP_NEWUSER_UNIQUENICK_INUSE;
				break;
			case TaskShared::EUserResponseType_UserExists:
				err_code = GP_NEWUSER_BAD_PASSWORD;
				break;
			case TaskShared::EUserResponseType_Profile_InvalidNick:
				err_code = GP_NEWUSER_BAD_NICK;
			break;
			case TaskShared::EUserResponseType_Profile_InvalidUniqueNick:
				err_code = GP_NEWUSER_UNIQUENICK_INVALID;
			break;
			case TaskShared::EUserResponseType_Success:
			break;
			default:
				err_code = GP_NEWUSER;
			break;
		}
		std::ostringstream s;
		s << "\\nur\\" << err_code;
		s << "\\pid\\" << profile.id;

		((Peer *)peer)->SendPacket(s.str().c_str());

		((Peer *)peer)->Delete();
	}
	void Peer::handle_newuser(OS::KVReader data_parser) {
		std::string nick;
		std::string uniquenick;
		std::string email;
		int partnercode = data_parser.GetValueInt("partnerid");
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
		req.profile_params.namespaceid = namespaceid;
		req.search_params.email = email;
		req.search_params.partnercode = partnercode;
		req.search_params.password = password;
		req.registerCallback = m_newuser_cb;

		TaskScheduler<TaskShared::UserRequest, TaskThreadData> *scheduler = ((SM::Server *)(GetDriver()->getServer()))->GetUserTask();
		scheduler->AddRequest(req.type, req);

	}
	void Peer::m_nick_email_auth_cb(bool success, OS::User user, OS::Profile profile, TaskShared::AuthData auth_data, void *extra, INetPeer *peer) {
		std::ostringstream s;
		s << "\\cur\\" << (int)success;
		s << "\\pid\\" << profile.id;

		((Peer *)peer)->SendPacket(s.str().c_str());

		((Peer *)peer)->Delete();
	}
	void Peer::handle_check(OS::KVReader data_parser) {

		std::string nick;
		std::string uniquenick;
		std::string email;
		int partnercode = data_parser.GetValueInt("partnerid");
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

		TaskShared::AuthRequest request;
		request.type = request.type = TaskShared::EAuthType_NickEmail;
		request.profile.nick = nick;

		if(data_parser.HasKey("namespaceid"))
			request.profile.namespaceid = namespaceid;

		request.user.email = email;
		request.user.password = password;

		if (data_parser.HasKey("partnerid"))
			request.user.partnercode = data_parser.GetValueInt("partnerid");
		request.extra = this;
		request.peer = this;
		request.callback = m_nick_email_auth_cb;
		IncRef();

		TaskScheduler<TaskShared::AuthRequest, TaskThreadData> *scheduler = ((SM::Server *)(GetDriver()->getServer()))->GetAuthTask();
		scheduler->AddRequest(request.type, request);
	}
	void Peer::handle_search(OS::KVReader data_parser) {
		TaskShared::ProfileRequest request;
		
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
		
		int namespace_id = data_parser.GetValueInt("namespaceid");
		if (namespace_id != 0) {
			request.namespaceids.push_back(namespace_id);
		}

		request.user_search_details.partnercode = data_parser.GetValueInt("partnerid");
		/*
		if(find_param("namespaceids", (char*)buf, (char*)&temp, GP_REASON_LEN)) {
			//TODO: namesiaceids\1,2,3,4,5
		}
		*/

		request.type = TaskShared::EProfileSearch_Profiles;
		request.extra = this;
		request.peer = this;
		IncRef();
		request.callback = Peer::m_search_callback;
		TaskScheduler<TaskShared::ProfileRequest, TaskThreadData> *scheduler = ((SM::Server *)(GetDriver()->getServer()))->GetProfileTask();
		scheduler->AddRequest(request.type, request);
	}
	void Peer::handle_searchunique(OS::KVReader data_parser) {
		TaskShared::ProfileRequest request;

		request.profile_search_details.id = 0;
		if (data_parser.HasKey("email")) {
			request.user_search_details.email = data_parser.GetValue("email");
		}
		if (data_parser.HasKey("nick")) {
			request.profile_search_details.nick = data_parser.GetValue("nick");
		}
		if (data_parser.HasKey("uniquenick")) {
			request.profile_search_details.uniquenick = data_parser.GetValue("uniquenick");
		}
		if (data_parser.HasKey("firstname")) {
			request.profile_search_details.firstname = data_parser.GetValue("firstname");
		}
		if (data_parser.HasKey("lastname")) {
			request.profile_search_details.lastname = data_parser.GetValue("lastname");
		}

		int namespace_id = data_parser.GetValueInt("namespaceid");
		if (namespace_id != 0) {
			request.namespaceids.push_back(namespace_id);
		}

		request.user_search_details.partnercode = data_parser.GetValueInt("partnerid");
		/*
		if(find_param("namespaceids", (char*)buf, (char*)&temp, GP_REASON_LEN)) {
			//TODO: namesiaceids\1,2,3,4,5
		}
		*/

		request.type = TaskShared::EProfileSearch_Profiles;
		request.extra = this;
		request.peer = this;
		IncRef();
		request.callback = Peer::m_search_callback;
		TaskScheduler<TaskShared::ProfileRequest, TaskThreadData> *scheduler = ((SM::Server *)(GetDriver()->getServer()))->GetProfileTask();
		scheduler->AddRequest(request.type, request);
	}
	void Peer::m_search_callback(TaskShared::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer) {
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
		((Peer *)peer)->SendPacket(s.str().c_str());

		((Peer *)peer)->Delete();
	}
	void Peer::m_search_buddies_callback(TaskShared::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer) {
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

		((Peer *)peer)->SendPacket(s.str().c_str());

		((Peer *)peer)->Delete();
	}
	void Peer::handle_others(OS::KVReader data_parser) {
		TaskShared::ProfileRequest request;
		int profileid = data_parser.GetValueInt("profileid");
		int namespaceid = data_parser.GetValueInt("namespaceid");

		request.profile_search_details.id = profileid;
		request.namespaceids.push_back(namespaceid);

		request.type = request.type = TaskShared::EProfileSearch_Buddies;
		request.extra = this;
		request.peer = this;
		IncRef();
		request.callback = Peer::m_search_buddies_callback;
		TaskScheduler<TaskShared::ProfileRequest, TaskThreadData> *scheduler = ((SM::Server *)(GetDriver()->getServer()))->GetProfileTask();
		scheduler->AddRequest(request.type, request);
	}

	void Peer::m_search_buddies_reverse_callback(TaskShared::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer) {
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

		((Peer *)peer)->SendPacket(s.str().c_str());

		((Peer *)peer)->Delete();
	}
	void Peer::handle_otherslist(OS::KVReader data_parser) {
		std::string pid_buffer;
		TaskShared::ProfileRequest request;

		int profileid = data_parser.GetValueInt("profileid");
		int namespaceid = data_parser.GetValueInt("namespaceid");

		request.profile_search_details.id = profileid;
		request.namespaceids.push_back(namespaceid);

		if(data_parser.HasKey("opids")) {
			std::string opids = data_parser.GetValue("opids");
/*			token = strtok(pid_buffer, "|");
			while(token != NULL) {
				request.target_profileids.push_back(atoi(token));
				token = strtok(NULL, "|");
			}

			free((void *)pid_buffer);*/
		}

		request.type = TaskShared::EProfileSearch_Buddies_Reverse;
		request.extra = this;
		request.peer = this;
		IncRef();
		request.callback = Peer::m_search_buddies_reverse_callback;
		TaskScheduler<TaskShared::ProfileRequest, TaskThreadData> *scheduler = ((SM::Server *)(GetDriver()->getServer()))->GetProfileTask();
		scheduler->AddRequest(request.type, request);
	}
	void Peer::m_search_valid_callback(TaskShared::EUserResponseType response_type, std::vector<OS::User> results, void *extra, INetPeer *peer) {
		std::ostringstream s;

		s << "\\vr\\" << ((results.size() > 0) ? 1 : 0);

		((Peer *)peer)->SendPacket(s.str().c_str());

		((Peer *)peer)->Delete();
	}
	void Peer::handle_valid(OS::KVReader data_parser) {
		TaskShared::UserRequest request;
		request.type = TaskShared::EUserRequestType_Search;
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
		request.peer = this;
		IncRef();
		request.callback = Peer::m_search_valid_callback;
		TaskScheduler<TaskShared::UserRequest, TaskThreadData> *scheduler = ((SM::Server *)(GetDriver()->getServer()))->GetUserTask();
		scheduler->AddRequest(request.type, request);
	}

	void Peer::handle_nicks(OS::KVReader data_parser) {
		TaskShared::ProfileRequest request;
		request.type = TaskShared::EProfileSearch_Profiles;
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

		request.user_search_details.password = password;

		request.extra = this;
		request.peer = this;
		IncRef();
		request.callback = Peer::m_nicks_cb;
		TaskScheduler<TaskShared::ProfileRequest, TaskThreadData> *scheduler = ((SM::Server *)(GetDriver()->getServer()))->GetProfileTask();
		scheduler->AddRequest(request.type, request);
	}
	void Peer::m_nicks_cb(TaskShared::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer) {
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

		((Peer *)peer)->SendPacket(s.str().c_str());
	}
	void Peer::send_error(GPShared::GPErrorCode code, std::string addon_data) {
		GPShared::GPErrorData error_data = GPShared::getErrorDataByCode(code);
		if (error_data.msg == NULL) {
			Delete();
			return;
		}
		std::ostringstream ss;
		ss << "\\error\\";
		ss << "\\err\\" << error_data.error;
		if (error_data.die) {
			ss << "\\fatal\\";
		}
		ss << "\\errmsg\\" << error_data.msg;
		if (addon_data.length())
			ss << addon_data;
		
		SendPacket(ss.str().c_str());
		if (error_data.die) {
			Delete();
		}
	}
	void Peer::SendPacket(std::string string, bool attach_final) {
		OS::Buffer buffer;
		//buffer.Write
		OS::LogText(OS::ELogLevel_Debug, "[%s] Send: %s\n", getAddress().ToString().c_str(), string.c_str());
		buffer.WriteBuffer((void *)string.c_str(), string.length());
		if (attach_final) {
			buffer.WriteBuffer((void *)"\\final\\", 7);
		}
		NetIOCommResp io_resp;
		io_resp = this->GetDriver()->getServer()->getNetIOInterface()->streamSend(m_sd, buffer);
		if (io_resp.disconnect_flag || io_resp.error_flag) {
			Delete();
		}
	}
	void Peer::Delete(bool timeout) {
		m_timeout_flag = timeout;
		m_delete_flag = true;
	}
}