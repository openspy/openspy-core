#ifndef _GPPEER_H
#define _GPPEER_H
#include "../main.h"
#include <OS/Auth.h>
#include <OS/User.h>
#include <OS/Profile.h>
#include <OS/Search/User.h>
#include <OS/Search/Profile.h>
#include <openssl/ssl.h>
#include <OS/KVReader.h>

#define FESL_READ_SIZE                  (16 * 1024)


#define FESL_PING_TIME 120

typedef struct _FESL_HEADER {
	uint32_t type;
	uint32_t subtype;
	uint32_t len;
} FESL_HEADER;

enum FESL_COMMAND_TYPE {
	FESL_TYPE_FSYS = 0x66737973,//0x73797366,
	FESL_TYPE_ACCOUNT = 0x61636374,//0x74636361,
	FESL_TYPE_SUBS = 0x73756273,
	FESL_TYPE_DOBJ = 0x646F626A,
};

namespace FESL {
	typedef struct _PeerStats {
		int total_requests;
		int version;

		long long bytes_in;
		long long bytes_out;

		int packets_in;
		int packets_out;

		OS::Address m_address;
		OS::GameData from_game;

		bool disconnected;
	} PeerStats;

	class Peer;
	typedef struct _CommandHandler {
		FESL_COMMAND_TYPE type;
		std::string command;
		bool (Peer::*mpFunc)(OS::KVReader);
	} CommandHandler;

	class Driver;
	class Peer : public INetPeer {
	public:
		Peer(Driver *driver, struct sockaddr_in *address_info, int sd);
		~Peer();
		
		void think(bool packet_waiting);
		const struct sockaddr_in *getAddress() { return &m_address_info; }

		int GetSocket() { return m_sd; };

		bool ShouldDelete() { return m_delete_flag; };
		bool IsTimeout() { return m_timeout_flag; }

		void SendPacket(FESL_COMMAND_TYPE type, std::string data, int force_sequence = -1);

		static OS::MetricValue GetMetricItemFromStats(PeerStats stats);
		OS::MetricInstance GetMetrics();
		PeerStats GetPeerStats() { if (m_delete_flag) m_peer_stats.disconnected = true; return m_peer_stats; };
	private:
		bool m_fsys_hello_handler(OS::KVReader kv_list);
		bool m_fsys_goodbye_handler(OS::KVReader kv_list);
		bool m_acct_login_handler(OS::KVReader kv_list);
		bool m_acct_get_account(OS::KVReader kv_list);
		bool m_acct_gamespy_preauth(OS::KVReader kv_list);
		bool m_subs_get_entitlement_by_bundle(OS::KVReader kv_list);
		bool m_acct_get_sub_accounts(OS::KVReader kv_list);
		bool m_acct_login_sub_account(OS::KVReader kv_list);
		bool m_dobj_get_object_inventory(OS::KVReader kv_list);
		void send_memcheck(int type, int salt = 0);
		bool m_openssl_accepted;
		SSL *m_ssl_ctx;
		int m_sequence_id;
		PeerStats m_peer_stats;

		static CommandHandler m_commands[];

		void ResetMetrics();

	};
}
#endif //_GPPEER_H