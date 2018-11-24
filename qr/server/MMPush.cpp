
#include "MMPush.h"

#include <OS/OpenSpy.h>
#include <OS/Config/AppConfig.h>
#include <OS/Redis.h>
#include "QRDriver.h"
#include "QRPeer.h"

#include <sstream>
#include <algorithm>

#include <OS/MessageQueue/rabbitmq/rmqConnection.h>

#define MM_PUSH_EXPIRE_TIME 1800

namespace MM {
	OS::TaskPool<MMPushTask, MMPushRequest> *m_task_pool = NULL;
	const char *mp_pk_name = "QRID";
	Redis::Connection *mp_redis_async_retrival_connection;

	MQ::IMQInterface *mp_mqconnection = NULL;

	const char *mm_channel_exchange = "openspy.master";

	const char *mm_client_message_routingkey = "client.message";
	const char *mm_server_event_routingkey = "server.event";

	MMPushTask::MMPushTask(int thread_index) {
		struct timeval t;
		t.tv_usec = 0;
		t.tv_sec = 60;

		m_thread_index = thread_index;

		mp_timer = OS::HiResTimer::makeTimer();
		m_thread_awake = false;

		mp_redis_connection = Redis::Connect(OS::g_redisAddress, t);

		mp_mutex = OS::CreateMutex();
		mp_thread = OS::CreateThread(MMPushTask::TaskThread, this, true);

	}
	MMPushTask::~MMPushTask() {
		mp_thread->SignalExit(true, mp_thread_poller);
		delete mp_thread;
		delete mp_mutex;
		delete mp_timer;

		Redis::Disconnect(mp_redis_connection);

	}
	void MMPushTask::AddDriver(QR::Driver *driver) {
		if (std::find(m_drivers.begin(), m_drivers.end(), driver) == m_drivers.end()) {
			m_drivers.push_back(driver);
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
		while (thread->isRunning() && (!task->m_request_list.empty() || task->mp_thread_poller->wait(MM_WAIT_MAX_TIME)) && thread->isRunning()) {
			task->mp_mutex->lock();
			task->m_thread_awake = true;
			while (!task->m_request_list.empty()) {
				MMPushRequest task_params = task->m_request_list.front();
				task->mp_mutex->unlock();
				task->mp_timer->start();
				switch (task_params.type) {
				case EMMPushRequestType_PushServer:
					task->PerformPushServer(task_params);
					break;
				case EMMPushRequestType_UpdateServer:
					task->PerformDeleteMissingKeysAndUpdateChanged(task_params);
					break;
				case EMMPushRequestType_UpdateServer_NoDiff:
					task->PerformUpdateServer(task_params);
					break;
				case EMMPushRequestType_DeleteServer:
					task->PerformDeleteServer(task_params);
					break;
				case EMMPushRequestType_GetGameInfoByGameName:
					task->PerformGetGameInfo(task_params);
					break;
				}
				task->mp_timer->stop();
				if (task_params.peer) {
					OS::LogText(OS::ELogLevel_Info, "[%s] Thread type %d - time: %f", OS::Address(task_params.peer->getAddress()).ToString().c_str(), task_params.type, task->mp_timer->time_elapsed() / 1000000.0);
				}
				task_params.peer->DecRef();
				task->mp_mutex->lock();
				task->m_request_list.pop();
			}
			task->m_thread_awake = false;
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
	void MMPushTask::PerformDeleteMissingKeysAndUpdateChanged(MMPushRequest request) {
		//find missing keys and push deletes
		std::map<std::string, std::string>::iterator it = request.old_server.m_keys.begin();
		std::map<std::string, std::vector<std::string> >::iterator it2;
		std::vector<std::string>::iterator it3;
		std::vector<std::string> missing_keys, missing_team_keys, missing_player_keys;

		Redis::Value v;
		Redis::Response resp;
		std::string name;
		int idx = 0;
		bool force_delete = false;
		bool change_occured = false;
		ServerInfo modified_server;

		modified_server.groupid = 0;

		while (it != request.old_server.m_keys.end()) {
			std::pair<std::string, std::string> p = *it;
			if(request.server.m_keys.find(p.first) == request.server.m_keys.end()) {
				missing_keys.push_back(p.first);
			}
			it++;
		}		

		it2 = request.old_server.m_player_keys.begin();
		while(it2 != request.old_server.m_player_keys.end()) {
			std::pair<std::string, std::vector<std::string> > p = *it2;
			if(request.server.m_player_keys.find(p.first) == request.server.m_player_keys.end()) {
				missing_player_keys.push_back(p.first);
			}
			it2++;
		}
		
		it2 = request.old_server.m_team_keys.begin();
		while(it2 != request.old_server.m_team_keys.end()) {
			std::pair<std::string, std::vector<std::string> > p = *it2;
			if(request.server.m_team_keys.find(p.first) == request.server.m_team_keys.end()) {
				missing_team_keys.push_back(p.first);
			}
			it2++;
		}

		
		std::ostringstream ss;
		ss << request.server.m_game.gamename << ":" << request.server.groupid << ":" << request.server.id << ":";
		std::string server_key = ss.str();



		Redis::Command(mp_redis_connection, 0, "SELECT %d", OS::ERedisDB_QR);
		it3 = missing_keys.begin();
		while(it3 != missing_keys.end()) {
			std::string s = *it3;
			Redis::Command(mp_redis_connection, 0, "HDEL %scustkeys %s", ss.str().c_str(),s.c_str());
			it3++;
		}

		ss.str("");
		while(true) {
			force_delete = false;
			if(request.server.m_player_keys.size() > 0) {
				if(idx > (*request.server.m_player_keys.begin()).second.size()) {
					force_delete = true;
				}
			}
			ss.str("");
			ss << server_key << "custkeys_player_" << idx++;
			resp = Redis::Command(mp_redis_connection, 0, "EXISTS %s", ss.str().c_str());
			v = resp.values.front();
			int ret = -1;
			if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
				ret = v.value._int;
			}
			else if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
				ret = atoi(v.value._str.c_str());
			}
			if(ret <= 0) {
				break;
			} else if(force_delete) {
				Redis::Command(mp_redis_connection, 0, "DEL %s", ss.str().c_str());
				break;
			}
			it3 = missing_player_keys.begin();
			while(it3 != missing_player_keys.end()) {
				name = *it3;
				Redis::Command(mp_redis_connection, 0, "HDEL %s %s", ss.str().c_str(), name.c_str());
				it3++;
			}
		}
		
		idx = 0;
		ss.str("");
		while(true) {
			force_delete = false;
			if(request.server.m_team_keys.size() > 0) {
				if(idx > (*request.server.m_team_keys.begin()).second.size()) {
					force_delete = true;
				}
			}
			ss.str("");
			ss << server_key << "custkeys_team_" << idx++;
			resp = Redis::Command(mp_redis_connection, 0, "EXISTS %s", ss.str().c_str());
			v = resp.values.front();
			int ret = -1;
			if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
				ret = v.value._int;
			}
			else if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
				ret = atoi(v.value._str.c_str());
			}
			if(ret <= 0) {
				break;
			} else if(force_delete) {
				Redis::Command(mp_redis_connection, 0, "DEL %s", ss.str().c_str());
				break;
			}
			it3 = missing_team_keys.begin();
			while(it3 != missing_team_keys.end()) {
				std::string name = *it3;
				Redis::Command(mp_redis_connection, 0, "HDEL %s %s", ss.str().c_str(), name.c_str());
				it3++;
			}
		}

		//create ServerInfo struct with changed keys only
		modified_server.m_game = request.server.m_game;
		modified_server.id = request.server.id;
		modified_server.m_address = request.server.m_address;
		it = request.server.m_keys.begin();
		while(it != request.server.m_keys.end()) {
			std::pair<std::string, std::string> p = *it;
			if(std::find(missing_keys.begin(), missing_keys.end(), p.first) == missing_keys.end()) {
				if(request.server.m_keys[p.first].compare(request.old_server.m_keys[p.first]) != 0) {
					modified_server.m_keys[p.first] = request.server.m_keys[p.first];
					change_occured = true;
				} 
			}
			it++;
		}

		it2 = request.server.m_player_keys.begin();
		while(it2 != request.server.m_player_keys.end()) {
			std::pair<std::string, std::vector<std::string> > p = *it2;
			if(std::find(missing_player_keys.begin(), missing_player_keys.end(), p.first) == missing_player_keys.end()) {
				it3 = p.second.begin();
				idx = 0;
				while(it3 != p.second.end()) {
					if(request.old_server.m_player_keys[p.first].size() > idx && request.server.m_player_keys[p.first].size() > idx && request.server.m_player_keys[p.first][idx].compare(request.old_server.m_player_keys[p.first][idx]) == 0) {
						modified_server.m_player_keys[p.first].push_back(std::string());
					} else {
						modified_server.m_player_keys[p.first].push_back(request.server.m_player_keys[p.first][idx]);
						change_occured = true;
					}
					idx++;
					it3++;
				}
			}
			it2++;
		}

		it2 = request.server.m_team_keys.begin();
		while(it2 != request.server.m_team_keys.end()) {
			std::pair<std::string, std::vector<std::string> > p = *it2;
			if(std::find(missing_team_keys.begin(), missing_team_keys.end(), p.first) == missing_team_keys.end()) {
				it3 = p.second.begin();
				idx = 0;
				while(it3 != p.second.end()) {
					if(request.old_server.m_team_keys.size() > idx && request.server.m_team_keys.size() > idx && request.server.m_team_keys[p.first][idx].compare(request.old_server.m_team_keys[p.first][idx]) == 0) {
						modified_server.m_team_keys[p.first].push_back(std::string());
					} else {
						modified_server.m_team_keys[p.first].push_back(request.server.m_team_keys[p.first][idx]);
						change_occured = true;
					}
					idx++;
					it3++;
				}
			}
			it2++;
		}
		PushServer(modified_server, false, request.server.id); //push to prevent expire

		if(change_occured) {
			std::ostringstream s;
			s << "\\update\\" << modified_server.m_game.gamename << ":" << modified_server.groupid << ":" << modified_server.id << ":";
			mp_mqconnection->sendMessage(mm_channel_exchange, mm_server_event_routingkey,s.str());
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

		if(pk_id == -1) {
			Redis::Command(mp_redis_connection, 0, "HSET %s gameid %d", server_key.c_str(), server.m_game.gameid);
			Redis::Command(mp_redis_connection, 0, "HSET %s id %d", server_key.c_str(), id);

			Redis::Command(mp_redis_connection, 0, "HDEL %s deleted", server_key.c_str()); //incase resume
		}

		
		std::string ipinput = server.m_address.ToString(true);



		Redis::Command(mp_redis_connection, 0, "SET IPMAP_%s-%d %s", ipinput.c_str(), server.m_address.GetPort(), server_key.c_str());
		Redis::Command(mp_redis_connection, 0, "EXPIRE IPMAP_%s-%d %d", ipinput.c_str(), server.m_address.GetPort(), MM_PUSH_EXPIRE_TIME);


		if(pk_id == -1) {
			Redis::Command(mp_redis_connection, 0, "HSET %s gameid %d", server_key.c_str(), server.m_game.gameid);
			Redis::Command(mp_redis_connection, 0, "HSET %s wan_port %d", server_key.c_str(), server.m_address.GetPort());
			Redis::Command(mp_redis_connection, 0, "HSET %s wan_ip \"%s\"", server_key.c_str(), ipinput.c_str());
		}
		else {
			Redis::Command(mp_redis_connection, 0, "ZINCRBY %s 1 \"%s\"", server.m_game.gamename.c_str(), server_key.c_str());
		}

		Redis::Command(mp_redis_connection, 0, "HINCRBY %s num_beats 1", server_key.c_str());


		Redis::Command(mp_redis_connection, 0, "EXPIRE %s %d", server_key.c_str(), MM_PUSH_EXPIRE_TIME);

		std::map<std::string, std::string>::iterator it = server.m_keys.begin();
		while (it != server.m_keys.end()) {
			std::pair<std::string, std::string> p = *it;
			Redis::Command(mp_redis_connection, 0, "HSET %scustkeys %s \"%s\"", server_key.c_str(), p.first.c_str(), OS::escapeJSON(p.second).c_str());
			it++;
		}
		Redis::Command(mp_redis_connection, 0, "EXPIRE %scustkeys %d", server_key.c_str(), MM_PUSH_EXPIRE_TIME);

		std::map<std::string, std::vector<std::string> >::iterator it2 = server.m_player_keys.begin();

		int i = 0, max_idx = 0;
		std::pair<std::string, std::vector<std::string> > p;
		std::vector<std::string>::iterator it3;
		while (it2 != server.m_player_keys.end()) {
			p = *it2;
			it3 = p.second.begin();
			while (it3 != p.second.end()) {
				std::string s = *it3;
				if(s.length() > 0) {
					Redis::Command(mp_redis_connection, 0, "HSET %scustkeys_player_%d %s \"%s\"", server_key.c_str(), i, p.first.c_str(), OS::escapeJSON(s).c_str());
				}
				i++;
				it3++;
			}
			max_idx= i;
			i = 0;
			it2++;
		}

		for(i=0;i<max_idx;i++) {
			Redis::Command(mp_redis_connection, 0, "EXPIRE %scustkeys_player_%d %d", server_key.c_str(), i, MM_PUSH_EXPIRE_TIME);
		}
		i=0;


		it2 = server.m_team_keys.begin();
		while (it2 != server.m_team_keys.end()) {
			p = *it2;
			it3 = p.second.begin();
			while (it3 != p.second.end()) {

				std::string s = *it3;
				if(s.length() > 0) {
					Redis::Command(mp_redis_connection, 0, "HSET %scustkeys_team_%d %s \"%s\"", server_key.c_str(), i, p.first.c_str(), OS::escapeJSON(s).c_str());
					i++;
				}
				it3++;
			}
			max_idx= i;
			i = 0;
			it2++;
		}
		for(i=0;i<max_idx;i++) {
			Redis::Command(mp_redis_connection, 0, "EXPIRE %scustkeys_team_%d %d", server_key.c_str(), i, MM_PUSH_EXPIRE_TIME);
		}
		i=0;

		Redis::Command(mp_redis_connection, 0, "SELECT %d", OS::ERedisDB_QR);
		if (publish) {
			Redis::Command(mp_redis_connection, 0, "ZADD %s %d \"%s\"", server.m_game.gamename.c_str(), pk_id, server_key.c_str());

			std::ostringstream s;
			s << "\\new\\" << server_key.c_str();
			mp_mqconnection->sendMessage(mm_channel_exchange, mm_server_event_routingkey, s.str());
		}

		return id;

	}
	void MMPushTask::UpdateServer(ServerInfo server) {
		//remove all keys and readd
		DeleteServer(server, false);
		PushServer(server, false, server.id);

		std::ostringstream s;
		s << "\\update\\" << server.m_game.gamename.c_str() << ":" << server.groupid << ":" << server.id << ":";
		mp_mqconnection->sendMessage(mm_channel_exchange, mm_server_event_routingkey,s.str());
	}
	void MMPushTask::DeleteServer(ServerInfo server, bool publish) {
		int groupid = server.groupid;
		int id = server.id;

		std::ostringstream ss;
		ss << server.m_game.gamename << ":" << server.groupid << ":" << server.id;
		std::string entry_name = ss.str();


		Redis::Command(mp_redis_connection, 0, "SELECT %d", OS::ERedisDB_QR);
		Redis::Command(mp_redis_connection, 0, "ZREM %s \"%s:%d:%d:\"", server.m_game.gamename.c_str(), server.m_game.gamename.c_str(), server.groupid, server.id);
		if (publish) {
			Redis::Response reply = Redis::Command(mp_redis_connection, 0, "HGET %s deleted", entry_name.c_str());
			Redis::Value v = reply.values[0];
			if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER && v.value._int == 1) {
				return;
			}
			else if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING && v.value._str.compare("1") == 0) {
				return;
			}
			Redis::Command(mp_redis_connection, 0, "HSET %s:%d:%d: deleted 1", server.m_game.gamename.c_str(), server.groupid, server.id);
			Redis::Command(mp_redis_connection, 0, "EXPIRE %s:%d:%d: %d", server.m_game.gamename.c_str(), server.groupid, server.id, MM_PUSH_EXPIRE_TIME);

			std::ostringstream s;
			s << "\\del\\" << server.m_game.gamename.c_str() << ":" << groupid << ":" << id << ":";
			mp_mqconnection->sendMessage(mm_channel_exchange, mm_server_event_routingkey,s.str());
		}
		else {
			Redis::Command(mp_redis_connection, 0, "DEL %s:%d:%d:", server.m_game.gamename.c_str(), server.groupid, server.id);
			Redis::Command(mp_redis_connection, 0, "DEL %s:%d:%d:custkeys", server.m_game.gamename.c_str(), server.groupid, server.id);

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
					Redis::Command(mp_redis_connection, 0, "DEL %s:%d:%d:custkeys_player_%d", server.m_game.gamename.c_str(), groupid, id, i);
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
					Redis::Command(mp_redis_connection, 0, "DEL %s:%d:%d:custkeys_team_%d", server.m_game.gamename.c_str(), groupid, id, i);
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
	void MMPushTask::MQListenerCallback(std::string message, void *extra) {
		std::string gamename, from_ip, to_ip, from_port, to_port, data, type;
		uint8_t *data_out;
		size_t data_len;

		QR::Server *server = (QR::Server *)extra;

		std::vector<std::string> vec = OS::KeyStringToVector(message);
		if (vec.size() < 1) return;
		type = vec.at(0);
		if (!type.compare("send_msg")) {
			if (vec.size() < 7) return;
			gamename = vec.at(1);
			from_ip = vec.at(2);
			from_port = vec.at(3);
			to_ip = vec.at(4);
			to_port = vec.at(5);
			data = vec.at(6);

			std::ostringstream ss;
			ss << to_ip << ":" << to_port;

			OS::Address address(ss.str().c_str());
			QR::Peer *peer = server->find_client(address);
			if (!peer) {
				return;							
			}
				
			OS::Base64StrToBin((const char *)data.c_str(), &data_out, data_len);
			peer->SendClientMessage(data_out, data_len);
			free(data_out);
		}
	}
	void SetupTaskPool(QR::Server* server) {

		std::string rabbitmq_address;
		OS::g_config->GetVariableString("", "rabbitmq_address", rabbitmq_address);

		std::string rabbitmq_user, rabbitmq_pass;
		OS::g_config->GetVariableString("", "rabbitmq_user", rabbitmq_user);
		OS::g_config->GetVariableString("", "rabbitmq_password", rabbitmq_pass);


		std::string rabbitmq_vhost;
		OS::g_config->GetVariableString("", "rabbitmq_vhost", rabbitmq_vhost);


		mp_mqconnection = (MQ::IMQInterface*)new MQ::rmqConnection(OS::Address(rabbitmq_address), rabbitmq_user, rabbitmq_pass, rabbitmq_vhost);
		mp_mqconnection->setReciever(mm_channel_exchange, mm_client_message_routingkey, MMPushTask::MQListenerCallback, "", server);

		struct timeval t;
		t.tv_usec = 0;
		t.tv_sec = 60;

		mp_redis_async_retrival_connection = Redis::Connect(OS::g_redisAddress, t);

		m_task_pool = new OS::TaskPool<MMPushTask, MMPushRequest>(NUM_MM_PUSH_THREADS);
		server->SetTaskPool(m_task_pool);
	}
	void Shutdown() {
		delete mp_mqconnection;
		Redis::Disconnect(mp_redis_async_retrival_connection);

		delete m_task_pool;
	}
	int MMPushTask::TryFindServerID(ServerInfo server) {
		std::string ip = server.m_address.ToString(true);
		std::stringstream map;
		map << "IPMAP_" << ip << "-" << server.m_address.GetPort();
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
			ret = -1;
			resp = Redis::Command(mp_redis_connection, 0, "GET %s", map.str().c_str());
			v = resp.values.front();
			if(resp.values.size() > 0) {
				std::string server_key; 
				if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
					server_key = v.value._str;
				}

				resp = Redis::Command(mp_redis_connection, 0, "HGET %s id", server_key.c_str());
				if(resp.values.size() > 0) {
					v = resp.values.front();
					if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
						ret = v.value._int;
					}
					else if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
						ret = atoi(v.value._str.c_str());
					}
				}
			}
			return ret;
		}
		else {
			return -1;
		}
	}
}
