#ifndef _GPPEER_H
#define _GPPEER_H
#include "../main.h"
#include <OS/Auth.h>
#include <OS/User.h>
#include <OS/Profile.h>
#include <OS/Search/Profile.h>

#define GPI_READ_SIZE                  (16 * 1024)

#define GP_NICK_LEN                 31
#define GP_UNIQUENICK_LEN           21
#define GP_FIRSTNAME_LEN            31
#define GP_LASTNAME_LEN             31
#define GP_EMAIL_LEN                51
#define GP_PASSWORD_LEN             31
#define GP_PASSWORDENC_LEN          ((((GP_PASSWORD_LEN+2)*4)/3)+1)
#define GP_HOMEPAGE_LEN             76
#define GP_ZIPCODE_LEN              11
#define GP_COUNTRYCODE_LEN          3
#define GP_PLACE_LEN                128
#define GP_AIMNAME_LEN              51
#define GP_REASON_LEN               1025
#define GP_STATUS_STRING_LEN        256
#define GP_LOCATION_STRING_LEN      256
#define GP_ERROR_STRING_LEN         256
#define GP_AUTHTOKEN_LEN            256
#define GP_PARTNERCHALLENGE_LEN     256
#define GP_CDKEY_LEN                65
#define GP_CDKEYENC_LEN             ((((GP_CDKEY_LEN+2)*4)/3)+1)
#define GP_LOGIN_TICKET_LEN         25

#define GP_RICH_STATUS_LEN          256
#define GP_STATUS_BASIC_STR_LEN     33

#define SP_PING_TIME 120

namespace SM {
	class Driver;
	class Peer {
	public:
		Peer(Driver *driver, struct sockaddr_in *address_info, int sd);
		~Peer();
		
		void think(bool packet_waiting);
		void handle_packet(char *data, int len);
		const struct sockaddr_in *getAddress() { return &m_address_info; }

		int GetSocket() { return m_sd; };

		bool ShouldDelete() { return m_delete_flag; };
		bool IsTimeout() { return m_timeout_flag; }

		void SendPacket(const uint8_t *buff, int len, bool attach_final = true);

	private:

		void handle_search(const char *buf, int len);
		static void m_search_callback(bool success, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra);

		static void m_search_buddies_callback(bool success, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra);
		void handle_others(const char *buf, int len);

		static void m_search_buddies_reverse_callback(bool success, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra);
		void handle_otherslist(const char *buf, int len);

		static void m_search_valid_callback(bool success, std::vector<OS::User> results, void *extra);
		void handle_valid(const char *buf, int len);

		static void m_nick_email_auth_cb(bool success, OS::User user, OS::Profile profile, OS::AuthData auth_data, void *extra);
		void handle_check(const char *buf, int len);

		static void m_newuser_cb(bool success, OS::User user, OS::Profile profile, OS::AuthData auth_data, void *extra);
		void handle_newuser(const char *buf, int len);

		int m_sd;
		Driver *mp_driver;

		struct sockaddr_in m_address_info;

		struct timeval m_last_recv, m_last_ping;

		bool m_delete_flag;
		bool m_timeout_flag;


	};
}
#endif //_GPPEER_H