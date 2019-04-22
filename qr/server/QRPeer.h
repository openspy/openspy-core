#ifndef _QRPEER_H
#define _QRPEER_H
#include "../main.h"
#include <OS/Net/NetPeer.h>
#include "tasks/tasks.h"

#define REQUEST_KEY_LEN 4
#define CHALLENGE_LEN 20
#define KV_MAX_LEN 64
#define HB_THROTTLE_TIME 10
namespace QR {
	class Driver;

	class Peer : public INetPeer {
	public:
		Peer(Driver *driver, INetIOSocket *sd, int version);
		virtual ~Peer();
		
		virtual void think(bool listener_waiting) = 0;
		virtual void handle_packet(INetIODatagram packet) = 0;

		bool ShouldDelete() { return m_delete_flag; };
		bool IsTimeout() { return m_timeout_flag; };
		void Delete(bool timeout = false);

		virtual void send_error(bool die, const char *fmt, ...) = 0;
		virtual void SendClientMessage(void *data, size_t data_len) = 0;
		


		virtual void OnGetGameInfo(OS::GameData game_info, int state) = 0;
		virtual void OnRegisteredServer(int pk_id) = 0;
		
		bool ServerDirty() { return m_server_info_dirty; };
		void SubmitDirtyServer();

		void DeleteServer();
		void OnConnectionReady();
	protected:

		bool isTeamString(const char *string);

		Driver *mp_driver;
		OS::CMutex *mp_mutex;

		struct timeval m_last_recv, m_last_ping, m_last_heartbeat;

		int m_version;

		bool m_server_pushed;
		bool m_sent_game_query;

		MM::ServerInfo m_server_info, m_dirty_server_info;
		bool m_server_info_dirty;

	};
}
#endif //_QRPEER_H