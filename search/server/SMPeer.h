#ifndef _GPPEER_H
#define _GPPEER_H
#include "../main.h"
#include <OS/Auth.h>
#include <OS/User.h>
#include <OS/Profile.h>
#include <OS/Search/User.h>
#include <OS/Search/Profile.h>
#include <OS/KVReader.h>
#include <OS/GPShared.h>

#include <OS/SharedTasks/Auth/AuthTasks.h>
#include <OS/SharedTasks/Account/UserTasks.h>
#include <OS/SharedTasks/Account/ProfileTasks.h>


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

enum {
	GP_MASK_NONE        =  0, // 0x00, Indicates that none of the profile's fields are visible.
	GP_MASK_HOMEPAGE    =  1, // 0x01, Indicates that the profile's homepage field is visible.
	GP_MASK_ZIPCODE     =  2, // 0x02, Indicates that the profile's zipcode field is visible.
	GP_MASK_COUNTRYCODE =  4, // 0x04, Indicates that the profile's country code field is visible.
	GP_MASK_BIRTHDAY    =  8, // 0x08, Indicates that the profile's birthday field is visible.
	GP_MASK_SEX         = 16, // 0x10, Indicates that the profile's sex field is visible.
	GP_MASK_EMAIL       = 32, // 0x20, Indicates that the profile's email field is visible.
	GP_MASK_BUDDYLIST	= 64, // 0x40, Indicates that the profile's buddy list is visible.
	GP_MASK_ALL         = -1, // 0xFFFFFFFF, Indicates that all of a profile's fields are visible.
};

#define SM_PING_TIME 120
#define MAX_UNPROCESSED_DATA 5000

namespace SM {
	class Driver;
	class Peer : public INetPeer {
	public:
		Peer(Driver *driver, INetIOSocket *sd);
		~Peer();
		
		void think(bool packet_waiting);
		void handle_packet(std::string data);

		bool ShouldDelete() { return m_delete_flag; };
		bool IsTimeout() { return m_timeout_flag; }

		void SendPacket(std::string string, bool attach_final = true);
		void send_error(GPShared::GPErrorCode code, std::string addon_data = "");

		void Delete(bool timeout = false);
	private:

		void handle_search(OS::KVReader data_parser);
		static void m_search_callback(TaskShared::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer);

		static void m_search_buddies_callback(TaskShared::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer);
		void handle_others(OS::KVReader data_parser);

		static void m_search_buddies_reverse_callback(TaskShared::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer);
		void handle_otherslist(OS::KVReader data_parser);

		static void m_search_valid_callback(TaskShared::EUserResponseType response_type, std::vector<OS::User> results, void *extra, INetPeer *peer);
		void handle_valid(OS::KVReader data_parser);

		static void m_nick_email_auth_cb(bool success, OS::User user, OS::Profile profile, TaskShared::AuthData auth_data, void *extra, INetPeer *peer);
		void handle_check(OS::KVReader data_parser);

		static void m_newuser_cb(bool success, OS::User user, OS::Profile profile, TaskShared::UserRegisterData auth_data, void *extra, INetPeer *peer);
		void handle_newuser(OS::KVReader data_parser);

		void handle_nicks(OS::KVReader data_parser);
		static void m_nicks_cb(TaskShared::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer);

		static const char *mp_hidden_str;

		OS::CMutex *mp_mutex;

		std::string m_kv_accumulator;

	};
}
#endif //_GPPEER_H