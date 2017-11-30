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
/*
20 = The user hasn't been authenticated yet, try again later. If the problem persists, contact customer support.
21 = custom error, use
"errorCode=21\n"
"errorContainer.0.fieldName=Account.BirthDate\n"

"errorContainer.0.fieldError=Unknown\n"
181 = The CD key is invalid
3004 = The CD key is invalid
182 = no error
180 = The CD key has already been registered with an account
163 = You can only have four (4) active Soldiers at the same time. If you want to create a new Soldier, you need to delete an existing one first.
121 = There were too many unauthorized login attempts to this account. Please contact EA support to resolve the issue.
122 = The specified password does not match the account password. Please try again.
123 = LOCERROR_gamenotregistered
140 = Too many password recoveries. Please contact EA support to resolve the issue.
141 = Too many account name recoveries. Please contact EA support to resolve the issue.
99 = A system error occured. Try again later. If problem persists, contact customer support.
101 = No account found with the specified name.
102 = The account has been disabled. Please contact EA support to resolve the issue.
103 = The account has been banned. Please contact EA support to resolve the issue
105 = LOCERROR_accountnotconfirmed
4294967131 = The age requirements for registering an account is not met
4294967192 = no error
4294967194 = The server timed out, please try again later.

custom error names:
Account.ScreenName = The account name was invalid. Please change and try again.
Account.Password = You have entered an invalid password. The password must contain only alphabetic and numeric characters, and must be between four and sixteen characters long. Passwords are case sensitive.
Account.EmailAddress = The specified email was invalid. Please change it and try again.
Account.Address.Zip = You have entered an invalid zip code. Please correct and try again.
Account.Address.Country = A country is required. Select a country from the list and try again.
Account.BirthDate = The birth date you specified is invalid. Make sure you type in the order specified. Please try again
Account.ParentalEmailAddress = Invalid parental email. Please correct and try again.

field error names:

0 = invalid
1 = required
2 = tooshort
3 = toolong
4 = mustbeginwithletter
5 = required
6 = invalid
7 = containsdisallowedtext
8 = invalid
9 = required
10 = required
11 = required
12 = exists
*/
enum FESL_ERROR {
	FESL_ERROR_AUTH_FAILURE = 122,
	FESL_ERROR_ACCOUNT_NOT_FOUND = 102,
	FESL_ERROR_ACCOUNT_EXISTS = 160,
	FESL_ERROR_NO_ERROR = 182,
	FESL_ERROR_SYSTEM_ERROR = 99,
	FESL_ERROR_AGE_RESTRICTION = 4294967131,
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

		void loginToSubAccount(std::string uniquenick);

		void SendError(FESL_COMMAND_TYPE type, FESL_ERROR error, std::string TXN);
	private:
		bool m_fsys_hello_handler(OS::KVReader kv_list);
		bool m_fsys_memcheck_handler(OS::KVReader kv_list);
		bool m_acct_gettos_handler(OS::KVReader kv_list);
		bool m_fsys_goodbye_handler(OS::KVReader kv_list);
		bool m_acct_nulogin_handler(OS::KVReader kv_list);
		bool m_acct_login_handler(OS::KVReader kv_list);
		bool m_acct_get_account(OS::KVReader kv_list);
		bool m_acct_gamespy_preauth(OS::KVReader kv_list);
		bool m_subs_get_entitlement_by_bundle(OS::KVReader kv_list);
		bool m_acct_get_sub_accounts(OS::KVReader kv_list);
		bool m_acct_login_sub_account(OS::KVReader kv_list);
		bool m_dobj_get_object_inventory(OS::KVReader kv_list);
		bool m_acct_add_sub_account(OS::KVReader kv_list);
		bool m_acct_disable_sub_account(OS::KVReader kv_list);
		bool m_acct_get_country_list(OS::KVReader kv_list);
		bool m_acct_add_account(OS::KVReader kv_list);
		bool m_acct_update_account(OS::KVReader kv_list);
		bool m_acct_register_game_handler(OS::KVReader kv_list);
		bool m_acct_send_account_name(OS::KVReader kv_list);
		bool m_acct_send_account_password(OS::KVReader kv_list);
		
		static void m_create_profile_callback(OS::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer);
		static void m_delete_profile_callback(OS::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer);
		static void m_update_user_callback(OS::EUserResponseType response_type, std::vector<OS::User> results, void *extra, INetPeer *peer);
		static void m_newuser_cb(bool success, OS::User user, OS::Profile profile, OS::AuthData auth_data, void *extra, int operation_id, INetPeer *peer);
		void send_memcheck(int type, int salt = 0);
		void send_subaccounts();
		bool m_openssl_accepted;
		SSL *m_ssl_ctx;
		int m_sequence_id;
		PeerStats m_peer_stats;
		OS::User m_user;
		OS::Profile m_profile;
		bool m_logged_in;
		bool m_pending_subaccounts;
		bool m_got_profiles;
		std::vector<OS::Profile> m_profiles;
		std::string m_session_key;
		void send_ping();

		static CommandHandler m_commands[];


		static void m_login_auth_cb(bool success, OS::User user, OS::Profile profile, OS::AuthData auth_data, void *extra, int operation_id, INetPeer *peer);
		static void m_nulogin_auth_cb(bool success, OS::User user, OS::Profile profile, OS::AuthData auth_data, void *extra, int operation_id, INetPeer *peer);
		static void m_create_auth_ticket(bool success, OS::User user, OS::Profile profile, OS::AuthData auth_data, void *extra, int operation_id, INetPeer *peer);
		static void m_search_callback(OS::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer);

		void ResetMetrics();

	};
}
#endif //_GPPEER_H