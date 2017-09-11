
#include "MMPush.h"
#include <OS/socketlib/socketlib.h>

#include <OS/Redis.h>
#include <OS/legacy/helpers.h>
#include "QRDriver.h"
#include "QRPeer.h"

#include <sstream>
#include <algorithm>

namespace MM {
	const char *sb_mm_channel = "serverbrowsing.servers";
	OS::TaskPool<MMPushTask, MMPushRequest> *m_task_pool = NULL;
	const char *mp_pk_name = "QRID";
	Redis::Connection *mp_redis_async_retrival_connection;
	OS::CThread *mp_async_thread;
	Redis::Connection *mp_redis_async_connection;
	MMPushTask *mp_async_lookup_task = NULL;

	//struct event_base *mp_event_base;
	void onRedisMessage(Redis::Connection *c, Redis::Response reply, void *privdata) {
	    char gamename[OS_MAX_GAMENAME+1],from_ip[32], to_ip[32], from_port[16], to_port[16], data[MAX_BASE64_STR+1], type[32];
		uint8_t *data_out;
		int data_len;
		Redis::Value v = reply.values.front();
		QR::Server *server = (QR::Server *)privdata;

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
						QR::Peer *peer = server->find_client(&address);
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
		t.tv_sec = 1;
		mp_redis_async_connection = Redis::Connect(OS_REDIS_ADDR, t);
		mp_async_lookup_task = new MMPushTask();
		Redis::LoopingCommand(mp_redis_async_connection, 0, onRedisMessage, thread->getParams(), "SUBSCRIBE %s", sb_mm_channel);
	    return NULL;
	}

	MMPushTask::MMPushTask() {
		struct timeval t;
		t.tv_usec = 0;
		t.tv_sec = 60;

		mp_redis_connection = Redis::Connect(OS_REDIS_ADDR, t);

		mp_mutex = OS::CreateMutex();
		mp_thread = OS::CreateThread(MMPushTask::TaskThread, this, true);
	}
	MMPushTask::~MMPushTask() {
		delete mp_thread;
		delete mp_mutex;

		Redis::Disconnect(mp_redis_connection);

	}
	void MMPushTask::AddDriver(QR::Driver *driver) {
		if (std::find(m_drivers.begin(), m_drivers.end(), driver) == m_drivers.end()) {
			m_drivers.push_back(driver);
		}
		if (this != mp_async_lookup_task) {
			mp_async_lookup_task->AddDriver(driver);
		}
	}
	void MMPushTask::RemoveDriver(QR::Driver *driver) {
		std::vector<QR::Driver *>::iterator it = std::find(m_drivers.begin(), m_drivers.end(), driver);
		if (it != m_drivers.end()) {
			m_drivers.erase(it);
		}
	}
	void *MMPushTask::TaskThread(OS::CThread *thread) {
		MMPushTask *task = (MMPushTask *)thread->getParams();
		while (task->mp_thread_poller->wait()) {
			task->mp_mutex->lock();
			MMPushRequest task_params = task->m_request_list.front();
			task->mp_mutex->unlock();
			switch (task_params.type) {
				case EMMPushRequestType_PushServer:
					task->PerformPushServer(task_params);
					break;
				case EMMPushRequestType_UpdateServer:
					task->PerformUpdateServer(task_params);
					break;
				case EMMPushRequestType_DeleteServer:
					task->PerformDeleteServer(task_params);
					break;
				case EMMPushRequestType_GetGameInfoByGameName:
					task->PerformGetGameInfo(task_params);
					break;
			}
			task->mp_mutex->lock();
			task_params.peer->DecRef();
			task->m_request_list.pop();
			task->mp_mutex->unlock();
		}
		return NULL;
	}
	void MMPushTask::PerformPushServer(MMPushRequest request) {
		int pk_id = PushServer(request.server, true, request.server.id);
		if (request.server.id != pk_id) {
			request.peer->OnRegisteredServer(pk_id, request.extra);
		}
	}
	void MMPushTask::PerformUpdateServer(MMPushRequest request) {
		DeleteServer(request.server, false);
		PushServer(request.server, false, request.server.id);
	}
	void MMPushTask::PerformDeleteServer(MMPushRequest request) {
		DeleteServer(request.server, true);
	}
	void MMPushTask::PerformGetGameInfo(MMPushRequest request) {
		OS::GameData game_info = OS::GetGameByName(request.gamename.c_str(), mp_redis_connection);
		request.peer->OnGetGameInfo(game_info, request.extra);
	}
	int MMPushTask::PushServer(ServerInfo server, bool publish, int pk_id) {
		int id = pk_id;
		int groupid = 0;

		if (id == -1) {
			id = TryFindServerID(server);
			if (id == -1) {
				id = GetServerID();
			}
		}

		server.id = id;
		server.groupid = groupid;

		std::ostringstream s;
		s << server.m_game.gamename << ":" << groupid << ":" << id << ":";
		std::string server_key = s.str();

		Redis::Command(mp_redis_connection, 0, "SELECT %d", OS::ERedisDB_QR);

		Redis::Command(mp_redis_connection, 0, "HSET %s gameid %d", server_key.c_str(), server.m_game.gameid);
		Redis::Command(mp_redis_connection, 0, "HSET %s id %d", server_key.c_str(), id);

		Redis::Command(mp_redis_connection, 0, "HDEL %s deleted", server_key.c_str()); //incase resume

		
		std::string ipinput = server.m_address.ToString(true);



		Redis::Command(mp_redis_connection, 0, "SET IPMAP_%s-%d %s", ipinput.c_str(), server.m_address.port, server_key.c_str());
		Redis::Command(mp_redis_connection, 0, "EXPIRE IPMAP_%s-%d 300", ipinput.c_str(), server.m_address.port);


		Redis::Command(mp_redis_connection, 0, "HSET %s gameid %d", server_key.c_str(), server.m_game.gameid);
		Redis::Command(mp_redis_connection, 0, "HSET %s wan_port %d", server_key.c_str(), server.m_address.port);
		Redis::Command(mp_redis_connection, 0, "HSET %s wan_ip \"%s\"", server_key.c_str(), ipinput.c_str());

		Redis::Command(mp_redis_connection, 0, "INCR %s num_beats", server_key.c_str());


		Redis::Command(mp_redis_connection, 0, "EXPIRE %s 300", server_key.c_str());

		std::map<std::string, std::string>::iterator it = server.m_keys.begin();
		while (it != server.m_keys.end()) {
			std::pair<std::string, std::string> p = *it;
			Redis::Command(mp_redis_connection, 0, "HSET %scustkeys %s \"%s\"", server_key.c_str(), p.first.c_str(), OS::escapeJSON(p.second).c_str());
			it++;
		}
		Redis::Command(mp_redis_connection, 0, "EXPIRE %scustkeys 300", server_key.c_str());

		std::map<std::string, std::vector<std::string> >::iterator it2 = server.m_player_keys.begin();

		int i = 0;
		std::pair<std::string, std::vector<std::string> > p;
		std::vector<std::string>::iterator it3;
		while (it2 != server.m_player_keys.end()) {
			p = *it2;
			it3 = p.second.begin();
			while (it3 != p.second.end()) {
				std::string s = *it3;
				Redis::Command(mp_redis_connection, 0, "HSET %scustkeys_player_%d %s \"%s\"", server_key.c_str(), i, p.first.c_str(), OS::escapeJSON(s).c_str());
				Redis::Command(mp_redis_connection, 0, "EXPIRE %scustkeys_player_%d 300", server_key.c_str(), i, p.first.c_str());
				i++;
				it3++;
			}
			i = 0;
			it2++;
		}


		it2 = server.m_team_keys.begin();
		while (it2 != server.m_team_keys.end()) {
			p = *it2;
			it3 = p.second.begin();
			while (it3 != p.second.end()) {

				std::string s = *it3;
				Redis::Command(mp_redis_connection, 0, "HSET %scustkeys_team_%d %s \"%s\"", server_key.c_str(), i, p.first.c_str(), OS::escapeJSON(s).c_str());
				Redis::Command(mp_redis_connection, 0, "EXPIRE %scustkeys_team_%d 300", server_key.c_str(), i);
				i++;
				it3++;
			}

			i = 0;
			it2++;
		}

		Redis::Command(mp_redis_connection, 0, "SELECT %d", OS::ERedisDB_QR);
		if (publish)
			Redis::Command(mp_redis_connection, 0, "PUBLISH %s \\new\\%s", sb_mm_channel, server_key.c_str());

		return id;

	}
	void MMPushTask::UpdateServer(ServerInfo server) {
		//remove all keys and readd
		DeleteServer(server, false);
		PushServer(server, false, server.id);

		Redis::Command(mp_redis_connection, 0, "PUBLISH %s \\update\\%s:%d:%d:", sb_mm_channel, server.m_game.gamename, server.groupid, server.id);
	}
	void MMPushTask::DeleteServer(ServerInfo server, bool publish) {
		int groupid = server.groupid;
		int id = server.id;
		Redis::Command(mp_redis_connection, 0, "SELECT %d", OS::ERedisDB_QR);
		if (publish) {
			Redis::Command(mp_redis_connection, 0, "HSET %s:%d:%d: deleted 1", server.m_game.gamename, server.groupid, server.id);
			Redis::Command(mp_redis_connection, 0, "EXPIRE %s:%d:%d: 300", server.m_game.gamename, server.groupid, server.id);
			Redis::Command(mp_redis_connection, 0, "PUBLISH %s \\del\\%s:%d:%d:", sb_mm_channel, server.m_game.gamename, groupid, id);
		}
		else {
			Redis::Command(mp_redis_connection, 0, "DEL %s:%d:%d:", server.m_game.gamename, server.groupid, server.id);
			Redis::Command(mp_redis_connection, 0, "DEL %s:%d:%d:custkeys", server.m_game.gamename, server.groupid, server.id);

			int i = 0;
			int groupid = server.groupid;
			int id = server.id;

			std::map<std::string, std::vector<std::string> >::iterator it2 = server.m_player_keys.begin();

			std::pair<std::string, std::vector<std::string> > p;
			std::vector<std::string>::iterator it3;
			while (it2 != server.m_player_keys.end()) {
				p = *it2;
				it3 = p.second.begin();
				while (it3 != p.second.end()) { //XXX: will be duplicate deletes but better than writing stuff to delete indivually atm, rewrite later though
					std::string s = *it3;
					Redis::Command(mp_redis_connection, 0, "DEL %s:%d:%d:custkeys_player_%d", server.m_game.gamename, groupid, id, i);
					i++;
					it3++;
				}

				i = 0;
				it2++;
			}


			it2 = server.m_team_keys.begin();
			while (it2 != server.m_team_keys.end()) {
				p = *it2;
				it3 = p.second.begin();
				while (it3 != p.second.end()) { //XXX: will be duplicate deletes but better than writing stuff to delete indivually atm, rewrite later though
					std::string s = *it3;
					Redis::Command(mp_redis_connection, 0, "DEL %s:%d:%d:custkeys_team_%d", server.m_game.gamename, groupid, id, i);
					i++;
					it3++;
				}

				i = 0;
				it2++;
			}
		}
	}
	int MMPushTask::GetServerID() {
		Redis::Command(mp_redis_connection, 0, "SELECT %d", OS::ERedisDB_QR);
		int ret = -1;
		Redis::Response resp = Redis::Command(mp_redis_connection, 1, "INCR %s", mp_pk_name);
		Redis::Value v = resp.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
			ret = v.value._int;
		}
		else if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			ret = atoi(v.value._str.c_str());
		}
		return ret;
	}
	void SetupTaskPool(QR::Server* server) {

		struct timeval t;
		t.tv_usec = 0;
		t.tv_sec = 60;

		mp_redis_async_retrival_connection = Redis::Connect(OS_REDIS_ADDR, t);
		mp_async_thread = OS::CreateThread(setup_redis_async, server, true);

		m_task_pool = new OS::TaskPool<MMPushTask, MMPushRequest>(NUM_MM_PUSH_THREADS);
		server->SetTaskPool(m_task_pool);
	}
	void Shutdown() {

	}
	int MMPushTask::TryFindServerID(ServerInfo server) {
		std::string ip = server.m_address.ToString(true);
		std::stringstream map;
		map << "IPMAP_" << ip << "-" << server.m_address.port;
		Redis::Command(mp_redis_connection, 0, "SELECT %d", OS::ERedisDB_QR);
		Redis::Response resp = Redis::Command(mp_redis_connection, 0, "EXISTS %s", map.str().c_str());
		Redis::Value v = resp.values.front();
		int ret = -1;
		if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
			ret = v.value._int;
		}
		else if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			ret = atoi(v.value._str.c_str());
		}
		if (ret == 1) {
			resp = Redis::Command(mp_redis_connection, 0, "HGET %s id", map.str().c_str());
			v = resp.values.front();
			if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
				ret = v.value._int;
			}
			else if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
				ret = atoi(v.value._str.c_str());
			}
			return ret;
		}
		else {
			return -1;
		}
	}
}
