#include "MMPush.h"
#include <OS/socketlib/socketlib.h>

#include <OS/Redis.h>
#include <OS/legacy/helpers.h>
#include "QRDriver.h"
#include "QRPeer.h"

#include <sstream>
#include <algorithm>

namespace MM {
	Redis::Connection *mp_redis_connection;
	const char *sb_mm_channel = "serverbrowsing.servers";

	const char *mp_pk_name = "QRID";
	QR::Driver *mp_driver;
	Redis::Connection *mp_redis_async_connection;
	OS::CThread *mp_thread;
	//struct event_base *mp_event_base;
	void onRedisMessage(Redis::Connection *c, Redis::Response reply, void *privdata) {
	    char gamename[OS_MAX_GAMENAME+1],from_ip[32], to_ip[32], from_port[16], to_port[16], data[MAX_BASE64_STR+1], type[32];
		uint8_t *data_out;
		int data_len;
		Redis::Value v = reply.values.front();

		char msg_type[16], server_key[64];
		if (v.type == Redis::REDIS_RESPONSE_TYPE_ARRAY) {
			if (v.arr_value.values.size() == 3 && v.arr_value.values[2].first == Redis::REDIS_RESPONSE_TYPE_STRING) {
				if (strcmp(v.arr_value.values[1].second.value._str.c_str(), sb_mm_channel) == 0) {
					char *temp_str = strdup(v.arr_value.values[2].second.value._str.c_str());
					find_param(0, temp_str, (char *)&type, sizeof(type) - 1);
					if (!strcmp(type, "send_msg")) {
						find_param(1, temp_str, (char *)&gamename, sizeof(gamename) - 1);
						find_param(2, temp_str, (char *)&from_ip, sizeof(from_ip) - 1);
						find_param(3, temp_str, (char *)&from_port, sizeof(from_port) - 1);
						find_param(4, temp_str, (char *)&to_ip, sizeof(to_ip) - 1);
						find_param(5, temp_str, (char *)&to_port, sizeof(to_port) - 1);
						find_param(6, temp_str, (char *)&data, sizeof(data) - 1);
						struct sockaddr_in address;
						address.sin_port = Socket::htons(atoi(to_port));
						address.sin_addr.s_addr = Socket::inet_addr((const char *)&to_ip);
						QR::Peer *peer = mp_driver->find_client(&address);
						if (!peer) {
							goto end_exit;
							
						}
							
						OS::Base64StrToBin((const char *)&data, &data_out, data_len);
						peer->SendClientMessage((uint8_t*)data_out, data_len);
						free(data_out);
					}
					end_exit:
						free((void *)temp_str);
				}
			}
		}
	}
	void *setup_redis_async(OS::CThread *thread) {
		struct timeval t;
		t.tv_usec = 0;
		t.tv_sec = 3;
		mp_redis_async_connection = Redis::Connect(OS_REDIS_ADDR, t);

		Redis::LoopingCommand(mp_redis_async_connection, 0, onRedisMessage, thread->getParams(), "SUBSCRIBE %s", sb_mm_channel);
	    return NULL;
	}
	void Init(QR::Driver *driver) {
		mp_driver = driver;
		struct timeval t;
		t.tv_usec = 0;
		t.tv_sec = 3;

		mp_redis_connection = Redis::Connect(OS_REDIS_ADDR, t);

	    mp_thread = OS::CreateThread(setup_redis_async, NULL, true);

	}
	void Shutdown() {
		delete mp_thread;
		Redis::Disconnect(mp_redis_connection);
		Redis::Disconnect(mp_redis_async_connection);
	}
	void PushServer(ServerInfo *server, bool publish, int pk_id) {
		int id = pk_id;
		int groupid = 0;

		if(id == -1) {
			id = GetServerID();
		}

		server->id = id;
		server->groupid = groupid;

		std::ostringstream s;
		s << server->m_game.gamename << ":" << groupid << ":" << id << ":";
		std::string server_key = s.str();

		Redis::Command(mp_redis_connection, 0, "SELECT %d", OS::ERedisDB_QR);

		Redis::Command(mp_redis_connection, 0, "HSET %s gameid %d",server_key.c_str(),server->m_game.gameid);
		Redis::Command(mp_redis_connection, 0, "HSET %s id %d",server_key.c_str(),id);


		struct sockaddr_in addr;
		addr.sin_port = Socket::htons(server->m_address.port);
		addr.sin_addr.s_addr = Socket::htonl(server->m_address.ip);
		const char *ipinput = Socket::inet_ntoa(addr.sin_addr);



		Redis::Command(mp_redis_connection, 0, "SET IPMAP_%s_%s-%d %s", server->m_game.gamename, ipinput, server->m_address.port, server_key.c_str());
		Redis::Command(mp_redis_connection, 0, "EXPIRE IPMAP_%s_%s-%d 300", server->m_game.gamename,ipinput, server->m_address.port);


		Redis::Command(mp_redis_connection, 0, "HSET %s gameid %d",server_key.c_str(),server->m_game.gameid);
		Redis::Command(mp_redis_connection, 0, "HSET %s wan_port %d",server_key.c_str(),server->m_address.port);
		Redis::Command(mp_redis_connection, 0, "HSET %s wan_ip \"%s\"",server_key.c_str(),ipinput);

		Redis::Command(mp_redis_connection, 0, "INCR %s num_beats",server_key.c_str());


		Redis::Command(mp_redis_connection, 0, "EXPIRE %s 300",server_key.c_str());

		std::map<std::string, std::string>::iterator it = server->m_keys.begin();
		while(it != server->m_keys.end()) {
			std::pair<std::string, std::string> p = *it;
			Redis::Command(mp_redis_connection, 0, "HSET %scustkeys %s \"%s\"",server_key.c_str(),p.first.c_str(),p.second.c_str());
			it++;
		}
		Redis::Command(mp_redis_connection, 0, "EXPIRE %scustkeys 300",server_key.c_str());

		//std::m,server->m_game.gamename,groupid,idap<std::string, std::vector<std::string> > m_player_keys;
		std::map<std::string, std::vector<std::string> >::iterator it2 = server->m_player_keys.begin();

		int i =0;
		std::pair<std::string, std::vector<std::string> > p;
		std::vector<std::string>::iterator it3;
		while(it2 != server->m_player_keys.end()) {
			p = *it2;
			it3 = p.second.begin();
			while(it3 != p.second.end()) {
				std::string s = *it3;
				Redis::Command(mp_redis_connection, 0, "HSET %scustkeys_player_%d %s \"%s\"",server_key.c_str(), i,p.first.c_str(),s.c_str());
				Redis::Command(mp_redis_connection, 0, "EXPIRE %scustkeys_player_%d 300",server_key.c_str(), i,p.first.c_str());
				i++;
				it3++;
			}
			i=0;
			it2++;
		}


		it2 = server->m_team_keys.begin();
		while(it2 != server->m_team_keys.end()) {
			p = *it2;
			it3 = p.second.begin();
			while(it3 != p.second.end()) {

				std::string s = *it3;
				Redis::Command(mp_redis_connection, 0, "HSET %scustkeys_team_%d %s \"%s\"",server_key.c_str(), i,p.first.c_str(),s.c_str());
				Redis::Command(mp_redis_connection, 0, "EXPIRE %scustkeys_team_%d 300",server_key.c_str(), i);
				i++;
				it3++;
			}

			i=0;
			it2++;
		}

		if(publish)
			Redis::Command(mp_redis_connection, 0, "PUBLISH %s \\new\\%s",sb_mm_channel,server_key.c_str());

	}
	void UpdateServer(ServerInfo *server) {
		//remove all keys and readd
		DeleteServer(server, false);
		PushServer(server, false, server->id);

		Redis::Command(mp_redis_connection, 0, "PUBLISH %s \\update\\%s:%d:%d:",sb_mm_channel,server->m_game.gamename,server->groupid,server->id);
	}
	void DeleteServer(ServerInfo *server, bool publish) {
		int groupid = server->groupid;
		int id = server->id;
		Redis::Command(mp_redis_connection, 0, "SELECT %d", OS::ERedisDB_QR);
		if(publish) {
			Redis::Command(mp_redis_connection, 0, "HSET %s:%d:%d: deleted 1",server->m_game.gamename,server->groupid,server->id);
			Redis::Command(mp_redis_connection, 0, "PUBLISH %s \\del\\%s:%d:%d:",sb_mm_channel,server->m_game.gamename,groupid,id);
		}
		else {
			Redis::Command(mp_redis_connection, 0, "DEL %s:%d:%d:",server->m_game.gamename,server->groupid,server->id);
			Redis::Command(mp_redis_connection, 0, "DEL %s:%d:%d:custkeys",server->m_game.gamename,server->groupid,server->id);

			int i =0;
			int groupid = server->groupid;
			int id = server->id;

			std::map<std::string, std::vector<std::string> >::iterator it2 = server->m_player_keys.begin();

			std::pair<std::string, std::vector<std::string> > p;
			std::vector<std::string>::iterator it3;
			while(it2 != server->m_player_keys.end()) {
				p = *it2;
				it3 = p.second.begin();
				while(it3 != p.second.end()) { //XXX: will be duplicate deletes but better than writing stuff to delete indivually atm, rewrite later though
					std::string s = *it3;
					Redis::Command(mp_redis_connection, 0, "DEL %s:%d:%d:custkeys_player_%d",server->m_game.gamename,groupid,id, i);
					i++;
					it3++;
				}

				i=0;
				it2++;
			}


			it2 = server->m_team_keys.begin();
			while(it2 != server->m_team_keys.end()) {
				p = *it2;
				it3 = p.second.begin();
				while(it3 != p.second.end()) { //XXX: will be duplicate deletes but better than writing stuff to delete indivually atm, rewrite later though
					std::string s = *it3;
					Redis::Command(mp_redis_connection, 0, "DEL %s:%d:%d:custkeys_team_%d",server->m_game.gamename,groupid,id, i);
					i++;
					it3++;
				}

				i=0;
				it2++;
			}
		}
	}
	int GetServerID() {
		Redis::Command(mp_redis_connection, 0, "SELECT %d", OS::ERedisDB_QR);
		Redis::Response resp = Redis::Command(mp_redis_connection, 0, "INCR %s", mp_pk_name);
		Redis::Value v = resp.values.front();
		int ret = -1;
		if(v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
			ret = v.value._int;
		}
		else if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			ret = atoi(v.value._str.c_str());
		}
		return ret;
	}
}
