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
		{ FESL_TYPE_FSYS, "Goodbye", &Peer::m_fsys_goodbye_handler },
		{ FESL_TYPE_ACCOUNT, "Login", &Peer::m_acct_login_handler},
		{ FESL_TYPE_SUBS, "GetEntitlementByBundle", &Peer::m_subs_get_entitlement_by_bundle },
		{ FESL_TYPE_ACCOUNT, "GetSubAccounts", &Peer::m_acct_get_sub_accounts },
		{ FESL_TYPE_DOBJ, "GetObjectInventory", &Peer::m_dobj_get_object_inventory },
		{ FESL_TYPE_ACCOUNT, "LoginSubAccount",  &Peer::m_acct_login_sub_account},
		{ FESL_TYPE_ACCOUNT, "GetAccount", &Peer::m_acct_get_account },
		{ FESL_TYPE_ACCOUNT, "GameSpyPreAuth", &Peer::m_acct_gamespy_preauth },
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
				goto end;
			}
			gettimeofday(&m_last_recv, NULL);
			buf[len] = 0;
			/*uint32_t subtype = htonl(header.subtype);
			if (subtype & 0x80000000 > m_sequence_id) {
				m_sequence_id = subtype & 0x0FFFFFFF;
			}*/
			OS::KVReader kv_data(buf, '=', '\n');
			printf("Got EAMsg(%d):\n%s\n", len, buf);
			printf("Seq ID: %08X\n", htonl(header.subtype));
			for (int i = 0; i < sizeof(m_commands) / sizeof(CommandHandler); i++) {
				if (Peer::m_commands[i].type == htonl(header.type)) {
					if (Peer::m_commands[i].command.compare(kv_data.GetValue("TXN")) == 0) {
						(*this.*Peer::m_commands[i].mpFunc)(kv_data);
						return;
					}
				}
			}
		}

		end:
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
		printf("Send: %s\n", data.c_str());
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
	bool Peer::m_acct_login_handler(OS::KVReader kv_list) {
		std::string kv_str = "TXN=Login\n"
			"lkey=VlUiXUWPBS\n"
			"nuid=hertc@example.com\n"
			"displayName=6929968\n"
			"profileId=636494100\n"
			"userId=636494100\n";
		SendPacket(FESL_TYPE_ACCOUNT, kv_str);
		return true;
	}
	bool Peer::m_acct_get_account(OS::KVReader kv_list) {
		std::ostringstream s;
		s << "TXN=GetAccount\n";
		s << "parentalEmail=parents@ea.com\n";
		s << "countryCode=US\n";
		s << "countryDesc=\"United States of America\"\n";
		s << "thirdPartyMailFlag=0\n";
		s << "dobDay=1\n";
		s << "dobMonth=1\n";
		s << "dobYear=1980\n";
		s << "name=Test\n";
		s << "email=Test@example.com\n";
		s << "profileID=636494100\n";
		s << "userId=636494100\n";
		s << "zipCode=90094\n";
		s << "gender=U\n";
		s << "eaMailFlag=0\n";
		SendPacket(FESL_TYPE_ACCOUNT, s.str());
		return true;
	}
	bool Peer::m_subs_get_entitlement_by_bundle(OS::KVReader kv_list) {
		std::string kv_str = "TXN=GetEntitlementByBundle\n"
			"EntitlementByBundle.[] = 0\n";
		SendPacket(FESL_TYPE_SUBS, kv_str);
		return true;
	}
	bool Peer::m_acct_get_sub_accounts(OS::KVReader kv_list) {
		std::string kv_str = "TXN=GetSubAccounts\n"
			"subAccounts.0=\"test123\"\n"
			"subAccounts.1=\"test666\"\n"
			"subAccounts.[]=2\n";
		SendPacket(FESL_TYPE_ACCOUNT, kv_str);
		return true;
	}
	bool Peer::m_dobj_get_object_inventory(OS::KVReader kv_list) {
		std::string kv_str = "TXN=GetObjectInventory\n"
			"ObjectInventory.[] = 0\n";
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
	bool Peer::m_acct_login_sub_account(OS::KVReader kv_list) {
		std::ostringstream s;
		s << "TXN=LoginSubAccount\n";
		s << "lkey=CC3GjhFQQCyEAc7o_Cn4qoAAAKDw.\n";
		s << "profileId=" << 636494100 << "\n";
		s << "userId=" << 636494100 << "\n";
		SendPacket(FESL_TYPE_ACCOUNT, s.str());
		return true;
	}
	bool Peer::m_acct_gamespy_preauth(OS::KVReader kv_list) {
		std::ostringstream s;
		s << "TXN=GameSpyPreAuth\n";
		//s << "challenge=pass\n";  // this is the password used in gs_login_server
		//s << "ticket=O%3d%3d%3d\n";   // base64 of ""
		s << "challenge=hzplukkz\n";
		s << "ticket=CC3GjhFQQCyEAc7o/GD5Zid8jtpGVE/EYNR1Gxh4KYVzQlMfd9FHsBwhxTNUW0MPKbr4bg90ckLkMflizg9iAACsg%3d%3d\n";
		SendPacket(FESL_TYPE_ACCOUNT, s.str());
		return true;
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