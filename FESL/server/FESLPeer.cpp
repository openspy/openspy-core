#include "FESLPeer.h"
#include "FESLDriver.h"
#include <OS/OpenSpy.h>
#include <OS/Search/Profile.h>
#include <OS/legacy/helpers.h>
#include <OS/legacy/buffreader.h>
#include <OS/legacy/buffwriter.h>
#include <OS/Search/User.h>
#include <OS/Search/Profile.h>
#include <OS/Auth.h>

#include <sstream>

namespace FESL {
	CommandHandler Peer::m_commands[] = {
		{ FESL_TYPE_FSYS, "Hello", &Peer::m_fsys_hello_handler },
		{ FESL_TYPE_FSYS, "MemCheck", &Peer::m_fsys_memcheck_handler },
		{ FESL_TYPE_FSYS, "Goodbye", &Peer::m_fsys_goodbye_handler },
		{ FESL_TYPE_SUBS, "GetEntitlementByBundle", &Peer::m_subs_get_entitlement_by_bundle },
		{ FESL_TYPE_DOBJ, "GetObjectInventory", &Peer::m_dobj_get_object_inventory },
		{ FESL_TYPE_ACCOUNT, "Login", &Peer::m_acct_login_handler },
		{ FESL_TYPE_ACCOUNT, "NuLogin", &Peer::m_acct_nulogin_handler },
		{ FESL_TYPE_ACCOUNT, "RegisterGame", &Peer::m_acct_register_game_handler },
		{ FESL_TYPE_ACCOUNT, "GetCountryList", &Peer::m_acct_get_country_list },
		{ FESL_TYPE_ACCOUNT, "GetTos", &Peer::m_acct_gettos_handler },
		{ FESL_TYPE_ACCOUNT, "GetSubAccounts", &Peer::m_acct_get_sub_accounts },
		{ FESL_TYPE_ACCOUNT, "LoginSubAccount",  &Peer::m_acct_login_sub_account },
		{ FESL_TYPE_ACCOUNT, "AddSubAccount",  &Peer::m_acct_add_sub_account },
		{ FESL_TYPE_ACCOUNT, "AddAccount",  &Peer::m_acct_add_account },
		{ FESL_TYPE_ACCOUNT, "UpdateAccount",  &Peer::m_acct_update_account },
		{ FESL_TYPE_ACCOUNT, "DisableSubAccount",  &Peer::m_acct_disable_sub_account },
		{ FESL_TYPE_ACCOUNT, "GetAccount", &Peer::m_acct_get_account },
		{ FESL_TYPE_ACCOUNT, "GameSpyPreAuth", &Peer::m_acct_gamespy_preauth },
		{ FESL_TYPE_ACCOUNT, "SendAccountName", &Peer::m_acct_send_account_name},
		{ FESL_TYPE_ACCOUNT, "SendPassword", &Peer::m_acct_send_account_password},
	};
	Peer::Peer(Driver *driver, struct sockaddr_in *address_info, int sd) : INetPeer(driver, address_info, sd) {
		m_delete_flag = false;
		m_timeout_flag = false;
		mp_mutex = OS::CreateMutex();
		ResetMetrics();
		gettimeofday(&m_last_ping, NULL);
		m_ssl_ctx = SSL_new(driver->getSSLCtx());
		SSL_set_fd(m_ssl_ctx, sd);
		OS::LogText(OS::ELogLevel_Info, "[%s] New connection", OS::Address(m_address_info).ToString().c_str());
		m_sequence_id = 1;
		m_openssl_accepted = false;
		m_logged_in = false;
		m_pending_subaccounts = false;
		m_got_profiles = false;
	}
	Peer::~Peer() {
		OS::LogText(OS::ELogLevel_Info, "[%s] Connection closed, timeout: %d", OS::Address(m_address_info).ToString().c_str(), m_timeout_flag);
		delete mp_mutex;
		SSL_free(m_ssl_ctx);
	}
	void Peer::think(bool packet_waiting) {
		char buf[FESL_READ_SIZE + 1];
		socklen_t slen = sizeof(struct sockaddr_in);
		int len = 0, piece_len = 0;
		if (m_delete_flag) return;
		if (packet_waiting) {
			if (!m_openssl_accepted) {
				if (SSL_accept(m_ssl_ctx) < 0) {
					OS::LogText(OS::ELogLevel_Info, "[%s] SSL accept failed", OS::Address(m_address_info).ToString().c_str());
					return;
				}
				OS::LogText(OS::ELogLevel_Info, "[%s] SSL accepted", OS::Address(m_address_info).ToString().c_str());
				m_openssl_accepted = true;
				send_memcheck(0);
				return;
			}

			FESL_HEADER header;
			if (m_ssl_ctx) {
				len = SSL_read(m_ssl_ctx, (char *)&header, sizeof(header));
			}
			else {
				len = recv(m_sd, (char *)&header, sizeof(header), 0);
			}			
			
			if (m_ssl_ctx) {
				len = SSL_read(m_ssl_ctx, (char *)&buf, htonl(header.len));
			}
			else {
				len = recv(m_sd, (char *)&buf, htonl(header.len), 0);
			}
			if (OS::wouldBlock()) {
				return;
			}
			if (len <= 0) {
				m_delete_flag = true;
				goto end;
			}
			gettimeofday(&m_last_recv, NULL);
			buf[len] = 0;
			/*uint32_t subtype = htonl(header.subtype);
			if (subtype & 0x80000000 > m_sequence_id) {
				m_sequence_id = subtype & 0x0FFFFFFF;
			}*/
			printf("got %s\n",buf);
			OS::KVReader kv_data(buf, '=', '\n');
			char *type;
			for (int i = 0; i < sizeof(m_commands) / sizeof(CommandHandler); i++) {
				if (Peer::m_commands[i].type == htonl(header.type)) {
					if (Peer::m_commands[i].command.compare(kv_data.GetValue("TXN")) == 0) {
						type = (char *)&Peer::m_commands[i].type;
						OS::LogText(OS::ELogLevel_Info, "[%s] Got Command: %c%c%c%c %s", OS::Address(m_address_info).ToString().c_str(), type[3], type[2], type[1], type[0], Peer::m_commands[i].command.c_str());
						(*this.*Peer::m_commands[i].mpFunc)(kv_data);
						return;
					}
				}
			}
			type = (char *)&header.type;
			OS::LogText(OS::ELogLevel_Info, "[%s] Got Unknown Command: %c%c%c%c %s", OS::Address(m_address_info).ToString().c_str(), type[3], type[2], type[1], type[0], kv_data.GetValue("TXN").c_str());
		}

		end:
		send_ping();
		//check for timeout
		struct timeval current_time;
		gettimeofday(&current_time, NULL);
		if(current_time.tv_sec - m_last_recv.tv_sec > FESL_PING_TIME*2) {
			m_delete_flag = true;
			m_timeout_flag = true;
		} else if(len == 0 && packet_waiting) {
			m_delete_flag = true;
		}
	}
	void Peer::send_ping() {
		//check for timeout
		struct timeval current_time;
		gettimeofday(&current_time, NULL);
		if(current_time.tv_sec - m_last_ping.tv_sec > FESL_PING_TIME) {
			gettimeofday(&m_last_ping, NULL);
			send_memcheck(0);
		}
	}
	void Peer::SendPacket(FESL_COMMAND_TYPE type, std::string data, int force_sequence) {
		FESL_HEADER header;
		if (force_sequence == -1) {
			header.subtype = htonl(0x80000000 | m_sequence_id++);
		}
		else {
			header.subtype = htonl(0x80000000 | force_sequence);
		}
		
		header.type = htonl(type);
		header.len = htonl(data.length() + sizeof(header) + 1);

		if (m_ssl_ctx) {
			SSL_write(m_ssl_ctx, &header, sizeof(header));
			SSL_write(m_ssl_ctx, data.c_str(), data.length()+1);
		}
		else {
			send(m_sd, (const char *)&header, sizeof(header), MSG_NOSIGNAL);
			send(m_sd, data.c_str(), data.length() + 1, MSG_NOSIGNAL);
		}
	}
	bool Peer::m_fsys_hello_handler(OS::KVReader kv_list) {
		/*
		TXN = Hello
		domainPartition.domain = eagames
		messengerIp = messaging.ea.com
		messengerPort = 13505
		domainPartition.subDomain = bf2142
		activityTimeoutSecs = 0
		curTime = "Nov-18-  17 02%3a19%3a07 UTC"
		theaterIp = bf2142 - pc.theater.ea.com
		theaterPort = 18305
		*/
		std::string kv_str = "TXN=Hello\ndomainPartition.domain=eagames\nmessengerIp=messaging.ea.com\nmessengerPort=13505\ndomainPartition.subDomain=bf2142\nactivityTimeoutSecs=0\ncurTime=\"Nov-18-  17 02%3a19%3a07 UTC\"\ntheaterIp=bf2142-pc.theater.ea.com\ntheaterPort=18305\n";
		SendPacket(FESL_TYPE_FSYS, kv_str);
		return true;
	}
	bool Peer::m_fsys_goodbye_handler(OS::KVReader kv_list) {
		m_delete_flag = true;
		return true;
	}
	bool Peer::m_fsys_memcheck_handler(OS::KVReader kv_list) {
		//send_memcheck(0);
		return true;
	}
	
	bool Peer::m_acct_register_game_handler(OS::KVReader kv_list) {
		std::string kv_str = "TXN=RegisterGame\n";
		SendPacket(FESL_TYPE_ACCOUNT, kv_str);
		return true;
	}
	bool Peer::m_acct_nulogin_handler(OS::KVReader kv_list) {
		OS::AuthTask::TryAuthEmailPassword(kv_list.GetValue("nuid"), OS_EA_PARTNER_CODE, kv_list.GetValue("password"), m_nulogin_auth_cb, NULL, 0, this);
		return true;
	}
	bool Peer::m_acct_login_handler(OS::KVReader kv_list) {
		OS::AuthTask::TryAuthUniqueNick_Plain(kv_list.GetValue("name"), OS_EA_PARTNER_CODE, 0, kv_list.GetValue("password"), m_login_auth_cb, NULL, 0, this);
		return true;
	}
	void Peer::m_nulogin_auth_cb(bool success, OS::User user, OS::Profile profile, OS::AuthData auth_data, void *extra, int operation_id, INetPeer *peer) {
		std::ostringstream s;
		if (success) {
			s << "TXN=NuLogin\n";
			s << "lkey=" << auth_data.session_key << "\n";
			((Peer *)peer)->m_session_key = auth_data.session_key;
			s << "displayName=" << profile.uniquenick << "\n";
			s << "userId=" << user.id << "\n";
			s << "profileId=" << profile.id << "\n";
			((Peer *)peer)->m_logged_in = true;
			((Peer *)peer)->m_user = user;
			((Peer *)peer)->m_profile = profile;
			((Peer *)peer)->SendPacket(FESL_TYPE_ACCOUNT, s.str());

			OS::ProfileSearchRequest request;
			request.type = OS::EProfileSearch_Profiles;
			request.user_search_details.id = user.id;
			request.peer = peer;
			peer->IncRef();
			request.callback = Peer::m_search_callback;
			OS::m_profile_search_task_pool->AddRequest(request);
		}
		else {
			((Peer *)peer)->SendError(FESL_TYPE_ACCOUNT, FESL_ERROR_AUTH_FAILURE, "NuLogin");
		}
	}
	void Peer::m_login_auth_cb(bool success, OS::User user, OS::Profile profile, OS::AuthData auth_data, void *extra, int operation_id, INetPeer *peer) {
		std::ostringstream s;
		if (success) {
			s << "TXN=Login\n";
			s << "lkey=" << auth_data.session_key << "\n";
			((Peer *)peer)->m_session_key = auth_data.session_key;
			s << "displayName=" << profile.uniquenick << "\n";
			s << "userId=" << user.id << "\n";
			s << "profileId=" << profile.id << "\n";
			((Peer *)peer)->m_logged_in = true;
			((Peer *)peer)->m_user = user;
			((Peer *)peer)->m_profile = profile;
			((Peer *)peer)->SendPacket(FESL_TYPE_ACCOUNT, s.str());

			OS::ProfileSearchRequest request;
			request.type = OS::EProfileSearch_Profiles;
			request.user_search_details.id = user.id;
			request.peer = peer;
			peer->IncRef();
			request.callback = Peer::m_search_callback;
			OS::m_profile_search_task_pool->AddRequest(request);
		}
		else {
			((Peer *)peer)->SendError(FESL_TYPE_ACCOUNT, FESL_ERROR_AUTH_FAILURE, "Login");
		}
	}
	void Peer::m_search_callback(OS::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer) {
		((Peer *)peer)->mp_mutex->lock();
		((Peer *)peer)->m_profiles = results;
		((Peer *)peer)->mp_mutex->unlock();
		((Peer *)peer)->m_got_profiles = true;
		if (((Peer *)peer)->m_pending_subaccounts) {
			((Peer *)peer)->m_pending_subaccounts = false;
			((Peer *)peer)->send_subaccounts();
		}		
	}
	bool Peer::m_acct_get_account(OS::KVReader kv_list) {
		std::ostringstream s;
		s << "TXN=GetAccount\n";
		s << "parentalEmail=parents@ea.com\n";
		s << "countryCode=US\n";
		s << "countryDesc=\"United States of America\"\n";
		s << "thirdPartyMailFlag=0\n";
		s << "dobDay=" << (int)m_profile.birthday.GetDay() << "\n";
		s << "dobMonth=" << (int)m_profile.birthday.GetMonth() << "\n";
		s << "dobYear=" << (int)m_profile.birthday.GetYear() << "\n";
		s << "name=Test\n";
		s << "email=" << m_user.email << "\n";
		s << "profileID=" << m_profile.id << "\n";
		s << "userId=" << m_user.id<< "\n";
		s << "zipCode=" << m_profile.zipcode << "\n";
		s << "gender=" << ((m_profile.sex == 0) ? 'M' : 'F') << "\n";
		s << "eaMailFlag=0\n";
		SendPacket(FESL_TYPE_ACCOUNT, s.str());
		return true;
	}
	bool Peer::m_subs_get_entitlement_by_bundle(OS::KVReader kv_list) {
		std::string kv_str = "TXN=GetEntitlementByBundle\n"
			"EntitlementByBundle.[]=0\n";
		SendPacket(FESL_TYPE_SUBS, kv_str);
		return true;
	}
	void Peer::send_subaccounts() {
		std::ostringstream s;
		mp_mutex->lock();
		std::vector<OS::Profile>::iterator it = m_profiles.begin();
		int i = 0;
		s << "TXN=GetSubAccounts\n";
		s << "subAccounts.[]=" << m_profiles.size() << "\n";
		while (it != m_profiles.end()) {
			OS::Profile profile = *it;
			s << "subAccounts." << i++ << "=\"" << profile.uniquenick << "\"\n";
			it++;
		}
		
		mp_mutex->unlock();
		SendPacket(FESL_TYPE_ACCOUNT, s.str());
	}
	void Peer::loginToSubAccount(std::string uniquenick) {
		std::ostringstream s;
		mp_mutex->lock();
		std::vector<OS::Profile>::iterator it = m_profiles.begin();
		while (it != m_profiles.end()) {
			OS::Profile profile = *it;
			if (profile.uniquenick.compare(uniquenick) == 0) {
				m_profile = profile;
				s << "TXN=LoginSubAccount\n";
				s << "lkey=" << m_session_key << "\n";
				s << "profileId=" << m_profile.id << "\n";
				s << "userId=" << m_user.id << "\n";
				SendPacket(FESL_TYPE_ACCOUNT, s.str());
				break;
			}
			it++;
		}
		mp_mutex->unlock();
	}
	bool Peer::m_acct_get_sub_accounts(OS::KVReader kv_list) {
		if (!m_got_profiles) {
			m_pending_subaccounts = true;
		}
		else {
			send_subaccounts();
		}
		return true;
	}
	bool Peer::m_dobj_get_object_inventory(OS::KVReader kv_list) {
		std::string kv_str = "TXN=GetObjectInventory\n"
			"ObjectInventory.[]=0\n";
		SendPacket(FESL_TYPE_DOBJ, kv_str);
		return true;
	}
	void Peer::send_memcheck(int type, int salt) {
		std::ostringstream s;
		s << "TXN=MemCheck\n";
		s << "memcheck.[]=0\n";
		s << "type=" << type << "\n";
		s << "salt=" << time(NULL) <<"\n";
		SendPacket(FESL_TYPE_FSYS, s.str(), 0);
	}
	bool Peer::m_acct_add_sub_account(OS::KVReader kv_list) {
		OS::ProfileSearchRequest request;
		std::string nick, oldnick;
		nick = kv_list.GetValue("name");

		request.user_search_details.id = m_user.id;
		//request.profile_search_details.id = m_profile.id;
		request.profile_search_details.nick = nick;
		request.profile_search_details.uniquenick = nick;
		request.extra = this;
		request.peer = this;
		request.peer->IncRef();
		request.type = OS::EProfileSearch_CreateProfile;
		request.callback = Peer::m_create_profile_callback;
		OS::m_profile_search_task_pool->AddRequest(request);
		
		return true;
	}
	void Peer::m_create_profile_callback(OS::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer) {
		if (response_reason == OS::EProfileResponseType_Success && results.size() > 0) {
			((Peer *)peer)->mp_mutex->lock();
			((Peer *)peer)->m_profiles.push_back(results.front());
			((Peer *)peer)->mp_mutex->unlock();
			((Peer *)peer)->SendError(FESL_TYPE_ACCOUNT, FESL_ERROR_NO_ERROR, "AddSubAccount");
		}
		else {
			((Peer *)peer)->SendError(FESL_TYPE_ACCOUNT, FESL_ERROR_SYSTEM_ERROR, "AddSubAccount");
		}
	}
	bool Peer::m_acct_disable_sub_account(OS::KVReader kv_list) {
		mp_mutex->lock();
		std::vector<OS::Profile>::iterator it = m_profiles.begin();
		while (it != m_profiles.end()) {
			OS::Profile profile = *it;
			if (profile.uniquenick.compare(kv_list.GetValue("name")) == 0) {
				OS::ProfileSearchRequest request;
				request.profile_search_details.id = profile.id;
				request.peer = this;
				request.extra = (void *)profile.id;
				request.peer->IncRef();
				request.type = OS::EProfileSearch_DeleteProfile;
				request.callback = Peer::m_delete_profile_callback;
				mp_mutex->unlock();
				OS::m_profile_search_task_pool->AddRequest(request);
				return true;
			}
			it++;
		}
		mp_mutex->unlock();
		return true;
	}
	void Peer::m_delete_profile_callback(OS::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer) {
		if (response_reason == OS::EProfileResponseType_Success) {
			((Peer *)peer)->mp_mutex->lock();
			std::vector<OS::Profile>::iterator it = ((Peer *)peer)->m_profiles.begin();
			while (it != ((Peer *)peer)->m_profiles.end()) {
				OS::Profile profile = *it;
				if (profile.id == (int)extra) {
					((Peer *)peer)->m_profiles.erase(it);
					break;
				}
				it++;
			}
			((Peer *)peer)->mp_mutex->unlock();

			std::ostringstream s;
			s << "TXN=DisableSubAccount\n";
			//((Peer *)peer)->SendPacket(FESL_TYPE_ACCOUNT, s.str());
			((Peer *)peer)->SendError(FESL_TYPE_ACCOUNT, (FESL_ERROR)0, "DisableSubAccount");
		}
		else {
			((Peer *)peer)->SendError(FESL_TYPE_ACCOUNT, (FESL_ERROR)FESL_ERROR_SYSTEM_ERROR, "DisableSubAccount");
		}
	}	
	bool Peer::m_acct_login_sub_account(OS::KVReader kv_list) {
		loginToSubAccount(kv_list.GetValue("name"));
		return true;
	}
	bool Peer::m_acct_gamespy_preauth(OS::KVReader kv_list) {
		OS::AuthTask::TryMakeAuthTicket(m_profile.id, m_create_auth_ticket, NULL, 0, this);
		return true;
	}
	bool Peer::m_acct_send_account_name(OS::KVReader kv_list) {
		return true;
	}
	bool Peer::m_acct_send_account_password(OS::KVReader kv_list) {
		return true;
	}
	void Peer::m_create_auth_ticket(bool success, OS::User user, OS::Profile profile, OS::AuthData auth_data, void *extra, int operation_id, INetPeer *peer) {
		std::ostringstream s;
		if (success) {
			s << "TXN=GameSpyPreAuth\n";
			/*s << "challenge=pass\n";  // this is the password used in gs_login_server
			//s << "ticket=O%3d%3d%3d\n";   // base64 of "" */
			if(auth_data.hash_proof.length())
				s << "challenge=" << auth_data.hash_proof << "\n";
			s << "ticket=" << auth_data.session_key << "\n";
			((Peer *)peer)->SendPacket(FESL_TYPE_ACCOUNT, s.str());
		}
		else {
			((Peer *)peer)->SendError(FESL_TYPE_ACCOUNT, FESL_ERROR_AUTH_FAILURE, "GameSpyPreAuth");
		}
	}
	void Peer::SendError(FESL_COMMAND_TYPE type, FESL_ERROR error, std::string TXN) {
		std::ostringstream s;
		s << "TXN=" << TXN << "\n";
		s << "errorContainer=[]\n";
		if (error == (FESL_ERROR)0) {
			s << "errorType=" << error << "\n";
		}
		s << "errorCode=" << error << "\n";
		SendPacket(type, s.str());
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

	bool Peer::m_acct_get_country_list(OS::KVReader kv_list) {
		std::ostringstream s;
		s << "TXN=GetCountryList\n";
		s << "countryList.0.description=\"North America\"\n";
		s << "countryList.0.ISOCode=1\n";
		SendPacket(FESL_TYPE_ACCOUNT, s.str());
		return true;
	}
	bool Peer::m_acct_add_account(OS::KVReader kv_list) {
		/*
		Got EAMsg(166):
		TXN=AddAccount
		name=thisisatest
		password=123321
		email=chc@test.com
		DOBDay=11
		DOBMonth=11
		DOBYear=1966
		zipCode=111231
		countryCode=1
		eaMailFlag=1
		thirdPartyMailFlag=1
		*/
		OS::User user;
		OS::Profile profile;
		user.email = kv_list.GetValue("email");
		user.password = kv_list.GetValue("password");
		profile.uniquenick = kv_list.GetValue("name");

		profile.birthday = OS::Date::Date(kv_list.GetValueInt("DOBYear"), kv_list.GetValueInt("DOBMonth"), kv_list.GetValueInt("DOBDay"));

		profile.namespaceid = 0;
		user.partnercode = OS_EA_PARTNER_CODE;
		OS::AuthTask::TryCreateUser_OrProfile(user, profile, true, m_newuser_cb, NULL, 0, this);
		
		return true;
	}
	void Peer::m_newuser_cb(bool success, OS::User user, OS::Profile profile, OS::AuthData auth_data, void *extra, int operation_id, INetPeer *peer) {
		FESL_ERROR err_code = FESL_ERROR_NO_ERROR;
		if (auth_data.response_code != -1 && auth_data.response_code != OS::LOGIN_RESPONSE_SUCCESS) {
			switch (auth_data.response_code) {
			case OS::CREATE_RESPONE_UNIQUENICK_IN_USE:
				err_code = FESL_ERROR_ACCOUNT_EXISTS;
				break;
			default:
				err_code = FESL_ERROR_SYSTEM_ERROR;
				break;
			}
			((Peer *)peer)->SendError(FESL_TYPE_ACCOUNT, (FESL_ERROR)err_code, "AddAccount");
		}
		else {
			std::ostringstream s;
			s << "TXN=AddAccount\n";
			s << "userId=" << user.id << "\n";
			s << "profileId=" << profile.id << "\n";
			((Peer *)peer)->SendPacket(FESL_TYPE_ACCOUNT, s.str());

		}
	}
	bool Peer::m_acct_update_account(OS::KVReader kv_list) {
		/*
		Got EAMsg(103):
		TXN=UpdateAccount
		email=chc@thmods.com
		parentalEmail=
		countryCode=1
		eaMailFlag=1
		thirdPartyMailFlag=0
		*/

		OS::Profile profile = m_profile;
		OS::User user = m_user;
		bool send_userupdate = false; //, send_profileupdate = false;

		if (kv_list.GetValue("email").compare(m_user.email) == 0) {
			user.email = kv_list.GetValue("email");
			send_userupdate = true;
		}
		/*
		OS::ProfileSearchRequest request;
		request.profile_search_details = m_profile;
		request.extra = NULL;
		request.peer = this;
		request.peer->IncRef();
		request.type = OS::EProfileSearch_UpdateProfile;
		request.callback = Peer::m_update_profile_callback;
		OS::m_profile_search_task_pool->AddRequest(request);
		*/

		if (send_userupdate) {
			OS::UserSearchRequest user_request;
			user_request.search_params = user;
			user_request.type = OS::EUserRequestType_Update;
			user_request.extra = NULL;
			user_request.peer = this;
			user_request.peer->IncRef();
			user_request.callback = Peer::m_update_user_callback;
			OS::m_user_search_task_pool->AddRequest(user_request);
		}
		return true;
	}
	void Peer::m_update_user_callback(OS::EUserResponseType response_reason, std::vector<OS::User> results, void *extra, INetPeer *peer) {
		std::ostringstream s;
		s << "TXN=UpdateAccount\n";
		if (response_reason == OS::EProfileResponseType_Success) {
			((Peer *)peer)->SendPacket(FESL_TYPE_ACCOUNT, s.str());
		}
		else {
			((Peer *)peer)->SendError(FESL_TYPE_ACCOUNT, FESL_ERROR_SYSTEM_ERROR, "UpdateAccount");
		}
	}
	bool Peer::m_acct_gettos_handler(OS::KVReader kv_list) {
		std::ostringstream s;
		s << "TXN=GetTos\n";
		s << "tos=\"Hi\"\n";
		SendPacket(FESL_TYPE_FSYS, s.str());
		return true;
	}
}