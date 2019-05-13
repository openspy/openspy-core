#ifndef _PEERCHATPEER_H
#define _PEERCHATPEER_H
#include "../main.h"
#include <OS/User.h>
#include <OS/Profile.h>
#include <OS/Mutex.h>

#include <OS/Net/NetPeer.h>
#include <OS/KVReader.h>

#include <OS/SharedTasks/Auth/AuthTasks.h>
#include <OS/SharedTasks/Account/ProfileTasks.h>


#define PEERCHAT_PING_TIME 300

namespace Peerchat {
	class Driver;

	class TaskResponse;

	class Peer;
	typedef void(Peer::*CommandCallback)(std::vector<std::string>);
	class CommandEntry {
	public:
		CommandEntry(std::string name, bool login_required, CommandCallback callback) {
			this->name = name;
			this->login_required = login_required;
			this->callback = callback;
		}
		std::string name;
		bool login_required;
		CommandCallback callback;
	};


	class Peer : public INetPeer {
	public:
		Peer(Driver *driver, INetIOSocket *sd);
		~Peer();

		void OnConnectionReady();
		void Delete(bool timeout = false);
		
		void think(bool packet_waiting);
		void handle_packet(OS::KVReader data_parser);

		int GetProfileID();

		bool ShouldDelete() { return m_delete_flag; };
		bool IsTimeout() { return m_timeout_flag; }

		void run_timed_operations();
		void SendPacket(const uint8_t *buff, size_t len);

		void RegisterCommands();
	private:
		static void OnNickReserve(TaskResponse response_data, Peer *peer);
		void handle_nick(std::vector<std::string> data_parser);
		void handle_user(std::vector<std::string> data_parser);

		OS::GameData m_game;
		Driver *mp_driver;
		
		struct timeval m_last_recv, m_last_ping;

		OS::User m_user;
		OS::Profile m_profile;

		OS::CMutex *mp_mutex;

		std::vector<CommandEntry> m_commands;
	};
}
#endif //_GPPEER_H