#include "main.h"
#include "MMQuery.h"
#include "server/SBDriver.h"
#include "server/SBServer.h"

#include <OS/Thread.h>

#include <sstream>
#include <algorithm>

#include <serverbrowsing/filter/filter.h>

#include <OS/Cache/GameCache.h>
#include <OS/KVReader.h>

namespace MM {

	OS::TaskPool<MMQueryTask, MMQueryRequest> *m_task_pool = NULL;
	OS::GameCache *m_game_cache;
	Redis::Connection *mp_redis_async_retrival_connection;
	OS::CThread *mp_async_thread;
	Redis::Connection *mp_redis_async_connection;
	MMQueryTask *mp_async_lookup_task = NULL;
	const char *sb_mm_channel = "serverbrowsing.servers";

	void *setup_redis_async(OS::CThread *thread) {
		struct timeval t;
		t.tv_usec = 0;
		t.tv_sec = 60;
		mp_redis_async_connection = Redis::Connect(OS::g_redisAddress, t);

		mp_async_lookup_task = new MMQueryTask(NUM_MM_QUERY_THREADS);

		Redis::LoopingCommand(mp_redis_async_connection, 60, MMQueryTask::onRedisMessage, thread->getParams(), "SUBSCRIBE %s", sb_mm_channel);
		return NULL;
	}

	void SetupTaskPool(SBServer *server) {

		struct timeval t;
		t.tv_usec = 0;
		t.tv_sec = 60;

		OS::DataCacheTimeout gameCacheTimeout;
		gameCacheTimeout.max_keys = 50;
		gameCacheTimeout.timeout_time_secs = 7200;

		mp_redis_async_retrival_connection = Redis::Connect(OS::g_redisAddress, t);
		mp_async_thread = OS::CreateThread(setup_redis_async, NULL, true);
		OS::Sleep(200);

		m_game_cache = new OS::GameCache(NUM_MM_QUERY_THREADS+1, gameCacheTimeout);

		m_task_pool = new OS::TaskPool<MMQueryTask, MMQueryRequest>(NUM_MM_QUERY_THREADS);
		server->SetTaskPool(m_task_pool);
	}

	void ShutdownTaskPool() {
		Redis::CancelLooping(mp_redis_async_connection);

		mp_async_thread->SignalExit(true);
		delete mp_async_thread;

		Redis::Disconnect(mp_redis_async_connection);		
		Redis::Disconnect(mp_redis_async_retrival_connection);		

		delete mp_async_lookup_task;
		delete m_task_pool;
		delete m_game_cache;
	}


	void MMQueryTask::onRedisMessage(Redis::Connection *c, Redis::Response reply, void *privdata) {
		MMQueryTask *task = (MMQueryTask *)mp_async_lookup_task;
		Redis::Value v = reply.values.front();

	    Server *server = NULL;

		std::string msg_type, server_key;
	    if (v.type == Redis::REDIS_RESPONSE_TYPE_ARRAY) {
	    	if(v.arr_value.values.size() == 3 && v.arr_value.values[2].first == Redis::REDIS_RESPONSE_TYPE_STRING) {
	    		if(v.arr_value.values[1].second.value._str.compare(sb_mm_channel) == 0) {
					std::vector<std::string> vec = OS::KeyStringToVector(v.arr_value.values[2].second.value._str);
					if (vec.size() < 2) return;
					msg_type = vec.at(0);
					server_key = vec.at(1);


	    			server = mp_async_lookup_task->GetServerByKey(server_key, mp_redis_async_retrival_connection, msg_type.compare("del") == 0);
	    			if(!server) return;

					MM::Server server_cpy = *server;
					delete server;
	    			std::vector<SB::Driver *>::iterator it = task->m_drivers.begin();
	    			while(it != task->m_drivers.end()) {
	    				SB::Driver *driver = *it;
			   			if(msg_type.compare("del") == 0) {
							driver->AddDeleteServer(server_cpy);
		    			} else if(msg_type.compare("new") == 0) {
							driver->AddNewServer(server_cpy);
		    			} else if(msg_type.compare("update") == 0) {
							driver->AddUpdateServer(server_cpy);
		    			}
	    				it++;
	    			}
					
	    		}
	    	}
	    }
	}
	void MMQueryTask::AddDriver(SB::Driver *driver) {
		if (std::find(m_drivers.begin(), m_drivers.end(), driver) == m_drivers.end()) {
			m_drivers.push_back(driver);
		}
		if (this != mp_async_lookup_task && mp_async_lookup_task) {
			mp_async_lookup_task->AddDriver(driver);
		}
	}
	void MMQueryTask::RemoveDriver(SB::Driver *driver) {
		std::vector<SB::Driver *>::iterator it = std::find(m_drivers.begin(), m_drivers.end(), driver);
		if (it != m_drivers.end()) {
			m_drivers.erase(it);
		}
	}
	MMQueryTask::MMQueryTask(int thread_index) {
		m_redis_timeout = 60;
		m_thread_awake = false;

		m_thread_index = thread_index;

		struct timeval t;
		t.tv_usec = 0;
		t.tv_sec = m_redis_timeout;

		mp_redis_connection = Redis::Connect(OS::g_redisAddress, t);

		mp_timer = OS::HiResTimer::makeTimer();
		mp_mutex = OS::CreateMutex();
		mp_thread = OS::CreateThread(MMQueryTask::TaskThread, this, true);

	}

	MMQueryTask::~MMQueryTask() {
		mp_thread->SignalExit(true, mp_thread_poller);

		delete mp_thread;
		delete mp_mutex;
		delete mp_timer;

		Redis::Disconnect(mp_redis_connection);

	}

	//////////////////////////////////////////////////
	/// Async MM Query code
	void MMQueryTask::AppendServerEntry(std::string entry_name, ServerListQuery *ret, bool all_keys, bool include_deleted, Redis::Connection *redis_ctx, const sServerListReq *req) {
		Redis::Response reply;
		int cursor = 0;
		int idx = 0;
		uint8_t last_type = Redis::REDIS_RESPONSE_TYPE_NULL;
		std::string key;
		std::ostringstream s;

		Redis::Value v, arr;

		std::map<std::string, std::string> all_cust_keys; //used for filtering

		if(!redis_ctx) {
			redis_ctx = mp_redis_connection;
		}

		/*
		XXX: add redis error checks, cleanup on error, etc
		*/


		//freeReplyObject(redisCommand(redis_ctx, "SELECT %d", OS::ERedisDB_QR));
		Redis::Command(redis_ctx, 0, "SELECT %d", OS::ERedisDB_QR);

		std::vector<std::string>::iterator it = ret->requested_fields.begin();

		//skip deleted servers
		if (!include_deleted) {
			reply = Redis::Command(redis_ctx, 0, "HGET %s deleted", entry_name.c_str());
			v = reply.values[0];
			if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER && v.value._int == 1) {
				return;
			}
			else if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING && v.value._str.compare("1") == 0) {
				return;
			}
		}


		Server *server = new MM::Server();

		reply = Redis::Command(redis_ctx, 0, "HGET %s gameid", entry_name.c_str());

		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
			goto error_cleanup;

		v = reply.values.front();
		if(v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			std::string gameid_reply = (v.value._str).c_str();
			int gameid = atoi(gameid_reply.c_str());
			if (req) {
				server->game = req->m_for_game;
			}
			else {				
				if (!m_game_cache->LookupGameByID(gameid, server->game)) {
					server->game = OS::GetGameByID(gameid, redis_ctx);
					m_game_cache->AddGame(m_thread_index, server->game);
				}				
			}
			
			Redis::Command(redis_ctx, 0, "SELECT %d", OS::ERedisDB_QR);
		}

		server->key = entry_name;

		server->kvFields["backend_id"] = entry_name;

		Redis::Command(redis_ctx, 0, "ZINCRBY %s -1 \"%s\"", server->game.gamename.c_str(), entry_name.c_str());


		reply = Redis::Command(redis_ctx, 0, "HGET %s id", entry_name.c_str());
		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
			goto error_cleanup;

		v = reply.values.front();

		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING)
			server->id = atoi((v.value._str).c_str());

		reply = Redis::Command(redis_ctx, 0, "HGET %s wan_port", entry_name.c_str());

		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
			goto error_cleanup;

		v = reply.values.front();

		if(v.type== Redis::REDIS_RESPONSE_TYPE_STRING)
			server->wan_address.port = atoi((v.value._str).c_str());

		reply = Redis::Command(redis_ctx, 0, "HGET %s wan_ip", entry_name.c_str());

		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
			goto error_cleanup;

		v = reply.values.front();

		if(v.type == Redis::REDIS_RESPONSE_TYPE_STRING)
			server->wan_address.ip = inet_addr((v.value._str).c_str());

		if(all_keys) {
			do {
				reply = Redis::Command(redis_ctx, 0, "HSCAN %scustkeys %d MATCH *", entry_name.c_str(), cursor);
				if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR || reply.values[0].arr_value.values.size() < 2)
					goto error_cleanup;

				v = reply.values[0].arr_value.values[0].second;
				arr = reply.values[0].arr_value.values[1].second;

				if (arr.type == Redis::REDIS_RESPONSE_TYPE_ARRAY) {
					if(v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
						cursor = atoi(v.value._str.c_str());
					} else if(v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
						cursor = v.arr_value.values[0].second.value._int;
					}

					for(int i=0;i<arr.arr_value.values.size();i+=2) {

						if(arr.arr_value.values[1].first != Redis::REDIS_RESPONSE_TYPE_STRING)
							continue;

						std::string key = arr.arr_value.values[i].second.value._str;
						if (arr.arr_value.values[i+1].first == Redis::REDIS_RESPONSE_TYPE_STRING) {
							server->kvFields[key] = (arr.arr_value.values[i + 1].second.value._str);
						}
						else if(arr.arr_value.values[i + 1].first == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
							server->kvFields[key] = arr.arr_value.values[i + 1].second.value._int;
						}

						if(std::find(ret->captured_basic_fields.begin(), ret->captured_basic_fields.end(), arr.arr_value.values[i + 1].second.value._str) == ret->captured_basic_fields.end()) {
							ret->captured_basic_fields.push_back(arr.arr_value.values[i].second.value._str);
						}
					}
				}
			} while (cursor != 0);
			cursor = 0;
			do {
				s << entry_name << "custkeys_player_" << idx;
				key = s.str();

				reply = Redis::Command(redis_ctx, 0, "EXISTS %s", key.c_str());
				if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
					break;

				v = reply.values.front();
				if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
					if (!v.value._int)
						break;
				}
				else if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
					if (!atoi(v.value._str.c_str()))
						break;
				}

				cursor = 0;
				do {
					reply = Redis::Command(redis_ctx, 0, "HSCAN %s %d MATCH *", key.c_str(), cursor);

					if (reply.values[0].arr_value.values.size() < 2) {
						goto error_cleanup;
					}

					v = reply.values[0].arr_value.values[0].second;
					arr = reply.values[0].arr_value.values[1].second;
					if (arr.arr_value.values.size() < 2)
						break;

					for (int i = 0; i<arr.arr_value.values.size(); i += 2) {

						if (arr.arr_value.values[1].first != Redis::REDIS_RESPONSE_TYPE_STRING)
							continue;

						std::string key = arr.arr_value.values[i].second.value._str;
						if (arr.arr_value.values[i + 1].first == Redis::REDIS_RESPONSE_TYPE_STRING) {
							server->kvPlayers[idx][key] = (arr.arr_value.values[i + 1].second.value._str);
						}
						else if (arr.arr_value.values[i + 1].first == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
							server->kvPlayers[idx][key] = arr.arr_value.values[i + 1].second.value._int;
						}

						if (std::find(ret->captured_team_fields.begin(), ret->captured_team_fields.end(), arr.arr_value.values[i].second.value._str) == ret->captured_team_fields.end()) {
							ret->captured_player_fields.push_back(arr.arr_value.values[i].second.value._str);
						}
					}
					s.str("");
				} while (cursor != 0);
				idx++;
			} while (true);
			s.str("");

			idx = 0;

			do {
				s << entry_name << "custkeys_team_" << idx;
				key = s.str();

				reply = Redis::Command(redis_ctx, 0, "EXISTS %s", key.c_str());
				if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
					break;

				v = reply.values.front();
				if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
					if (!v.value._int)
						break;
				}
				else if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
					if (!atoi(v.value._str.c_str()))
						break;
				}

				cursor = 0;
				do {
					reply = Redis::Command(redis_ctx, 0, "HSCAN %s %d MATCH *", key.c_str(), cursor);
					if (reply.values.size() < 2|| reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR || reply.values[0].arr_value.values.size() < 2)
						break;

					v = reply.values[0];
					arr = reply.values[1];

					if (v.type == Redis::REDIS_RESPONSE_TYPE_ARRAY) {
						if (v.arr_value.values.size() <= 0) {
							break;
						}
						for (int i = 0; i<arr.arr_value.values.size(); i += 2) {

							if (arr.arr_value.values[1].first != Redis::REDIS_RESPONSE_TYPE_STRING)
								continue;

							std::string key = arr.arr_value.values[i].second.value._str;
							if (arr.arr_value.values[i + 1].first == Redis::REDIS_RESPONSE_TYPE_STRING) {
								server->kvTeams[idx][key] = (arr.arr_value.values[i + 1].second.value._str);
							}
							else if (arr.arr_value.values[i + 1].first == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
								server->kvTeams[idx][key] = arr.arr_value.values[i + 1].second.value._int;
							}

							if(std::find(ret->captured_team_fields.begin(), ret->captured_team_fields.end(), arr.arr_value.values[i].second.value._str) == ret->captured_team_fields.end()) {
								ret->captured_team_fields.push_back(arr.arr_value.values[i].second.value._str);
							}
						}
					}
					s.str("");
				} while(cursor != 0);
				idx++;
			} while(true);

			idx = 0;
			all_cust_keys = server->kvFields;
		} else {
			do {
				reply = Redis::Command(redis_ctx, 0, "HSCAN %scustkeys %d MATCH *", entry_name.c_str(), cursor);
				if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR || reply.values[0].arr_value.values.size() < 2)
					goto error_cleanup;

				v = reply.values[0].arr_value.values[0].second;
				arr = reply.values[0].arr_value.values[1].second;

				if (arr.type == Redis::REDIS_RESPONSE_TYPE_ARRAY) {
					if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
						cursor = atoi(v.value._str.c_str());
					}
					else if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
						cursor = v.arr_value.values[0].second.value._int;
					}

					for (int i = 0; i<arr.arr_value.values.size(); i += 2) {

						if (arr.arr_value.values[1].first != Redis::REDIS_RESPONSE_TYPE_STRING)
							continue;

						std::string key = arr.arr_value.values[i].second.value._str;
						if (arr.arr_value.values[i + 1].first == Redis::REDIS_RESPONSE_TYPE_STRING) {
							server->kvFields[key] = (arr.arr_value.values[i + 1].second.value._str);
						}
						else if (arr.arr_value.values[i + 1].first == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
							server->kvFields[key] = arr.arr_value.values[i + 1].second.value._int;
						}

						if (std::find(ret->captured_basic_fields.begin(), ret->captured_basic_fields.end(), arr.arr_value.values[i].second.value._str) == ret->captured_basic_fields.end()) {
							ret->captured_basic_fields.push_back(arr.arr_value.values[i].second.value._str);
						}
					}
				}
			} while(cursor != 0);


			all_cust_keys = server->kvFields;
			server->kvFields.clear(); //remove all keys
			std::map<std::string, std::string>::iterator it = all_cust_keys.begin();
			while(it != all_cust_keys.end()) {
				std::pair<std::string, std::string> p = *it;

				//add only keys which were requested
				if(std::find(ret->requested_fields.begin(), ret->requested_fields.end(), p.first) != ret->requested_fields.end()) {
					server->kvFields[p.first] = p.second;
				}
				it++;
			}
		}

		if(!req || filterMatches(req->filter.c_str(), all_cust_keys)) {
			if (!req || (ret->list.size() < req->max_results || req->max_results == 0)) {
				ret->list.push_back(server);
			}
		}
		else {
			delete server;
		}
		goto true_exit;

	error_cleanup:
			delete server;
	true_exit:
		Redis::Command(redis_ctx, 0, "SELECT %d", OS::ERedisDB_QR);

	}
	bool MMQueryTask::FindAppend_PlayerKVFields(Server *server, std::string entry_name, std::string key, int index, Redis::Connection *redis_ctx)
	 {
		Redis::Response reply;
		Redis::Value v;
		reply = Redis::Command(redis_ctx, 0, "HGET %s %s", entry_name.c_str(), key.c_str());
		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
			return false;

		v = reply.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			server->kvPlayers[index][key] = (v.value._str);
		}
		if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
			server->kvPlayers[index][key] = v.value._int;
		}

		return true;

	}
	bool MMQueryTask::FindAppend_TeamKVFields(Server *server, std::string entry_name, std::string key, int index, Redis::Connection *redis_ctx) {
		Redis::Response reply;
		Redis::Value v;
		reply = Redis::Command(redis_ctx, 0, "HGET %s %s", entry_name.c_str(), key.c_str());
		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
			return false;

		v = reply.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			server->kvTeams[index][key] = (v.value._str);
		}
		if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
			server->kvTeams[index][key] = v.value._int;
		}

		return true;

	}
	bool MMQueryTask::FindAppend_ServKVFields(Server *server, std::string entry_name, std::string key, Redis::Connection *redis_ctx) {
		Redis::Response reply;
		Redis::Value v;
		reply = Redis::Command(redis_ctx, 0, "HGET %s %s", entry_name.c_str(), key.c_str());
		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
			return false;

		v = reply.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			server->kvFields[key] = (v.value._str);
		}
		if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
			server->kvFields[key] = v.value._int;
		}

		return true;

	}
	void MMQueryTask::AppendGroupEntry(const char *entry_name, ServerListQuery *ret, Redis::Connection *redis_ctx, bool all_keys, const MMQueryRequest *request) {
		Redis::Response reply;
		Redis::Value v, arr;
		int cursor = 0;
		std::map<std::string, std::string> all_cust_keys; //used for filtering

		/*
		XXX: add redis error checks, cleanup on error, etc
		*/

		std::vector<std::string>::iterator it = ret->requested_fields.begin();
		Server *server = new MM::Server();

		Redis::Command(redis_ctx, 0, "SELECT %d", OS::ERedisDB_SBGroups);

		reply = Redis::Command(redis_ctx, 0, "HGET %s gameid", entry_name);
		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
			goto error_cleanup;

		v = reply.values.front();

		if (request) {
			server->game = request->req.m_for_game;
		}
		else {
			if (!m_game_cache->LookupGameByID(atoi((v.value._str).c_str()), server->game)) {
				server->game = OS::GetGameByID(atoi((v.value._str).c_str()), redis_ctx);
				m_game_cache->AddGame(m_thread_index, server->game);
			}
		}
		
		Redis::Command(redis_ctx, 0, "SELECT %d", OS::ERedisDB_SBGroups); //change context back to SB db id

		reply = Redis::Command(redis_ctx, 0, "HGET %s groupid", entry_name);
		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
			goto error_cleanup;

		v = reply.values.front();

		server->wan_address.ip = htonl(atoi((v.value._str).c_str())); //for V2
		server->kvFields["groupid"] = (v.value._str).c_str(); //for V1
		
		FindAppend_ServKVFields(server, entry_name, "maxwaiting", redis_ctx);
		FindAppend_ServKVFields(server, entry_name, "hostname", redis_ctx);
		FindAppend_ServKVFields(server, entry_name, "password", redis_ctx);
		FindAppend_ServKVFields(server, entry_name, "numwaiting", redis_ctx);
		FindAppend_ServKVFields(server, entry_name, "numservers", redis_ctx);

		if(all_keys) {
	
			do {
				reply = Redis::Command(redis_ctx, 0, "HSCAN %scustkeys %d match *", entry_name);
				if (reply.values.size() < 1 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR || reply.values[0].arr_value.values.size() < 2)
					goto error_cleanup;

					v = reply.values[0].arr_value.values[0].second;
					arr = reply.values[0].arr_value.values[1].second;
					if (arr.type == Redis::REDIS_RESPONSE_TYPE_ARRAY) {
						if(v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
					 		cursor = atoi(v.value._str.c_str());
					 	} else if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
					 		cursor = v.value._int;
					 	}

						if(arr.arr_value.values.size() <= 0) {
							break;
						}

						for(int i=0;i<arr.arr_value.values.size();i++) {
							std::string search_key = entry_name;
							search_key += "custkeys";
							FindAppend_ServKVFields(server, search_key, arr.arr_value.values[i].second.value._str, redis_ctx);
							if(std::find(ret->captured_basic_fields.begin(), ret->captured_basic_fields.end(), arr.arr_value.values[i].second.value._str) == ret->captured_basic_fields.end()) {
								ret->captured_basic_fields.push_back(arr.arr_value.values[i].second.value._str);
							}
						}
					}
			} while(cursor != 0);

		} else {

			while (it != ret->requested_fields.end()) {
				std::string field = *it;
				std::string entry = entry_name;
				entry += "custkeys";
				FindAppend_ServKVFields(server, entry, field, redis_ctx);
				it++;
			}
		}

		all_cust_keys = server->kvFields;
		if (!request || filterMatches(request->req.filter.c_str(), all_cust_keys)) {
			if (!request || (ret->list.size() < request->req.max_results || request->req.max_results == 0)) {
				ret->list.push_back(server);
			}
		}			
		return;

	error_cleanup:
		delete server;

	}
	ServerListQuery MMQueryTask::GetServers(const sServerListReq *req, const MMQueryRequest *request) {
		ServerListQuery ret;

		Redis::Response reply;
		Redis::Value v, arr;

		ret.requested_fields = req->field_list;

		Redis::Command(mp_redis_connection, 0, "SELECT %d", OS::ERedisDB_QR);
		
		int cursor = 0;
		bool sent_servers = false;

		do {
			ServerListQuery streamed_ret;
			streamed_ret.requested_fields = ret.requested_fields;
			reply = Redis::Command(mp_redis_connection, 0, "ZSCAN %s %d", req->m_for_game.gamename.c_str(), cursor);
			if (Redis::CheckError(reply))
				goto error_cleanup;

			streamed_ret.last_set = false;
			if (cursor == 0) {
				streamed_ret.first_set = true;
			} else {
				streamed_ret.first_set = false;
			}
			if (reply.values[0].arr_value.values.size() < 2) {
				goto error_cleanup;
			}
			v = reply.values[0].arr_value.values[0].second;
			arr = reply.values[0].arr_value.values[1].second;

		 	if(v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
		 		cursor = atoi(v.value._str.c_str());
		 	} else if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
		 		cursor = v.value._int;
		 	}

			if (cursor == 0) {
				streamed_ret.last_set = true;
			}

			for(int i=0;i<arr.arr_value.values.size();i+=2) {
				std::string server_key = arr.arr_value.values[i].second.value._str;
				reply = Redis::Command(mp_redis_connection, 0, "EXISTS %s", server_key.c_str());
				
				if (Redis::CheckError(reply) || reply.values.size() == 0) {
					continue;
				}
				else {
					v = reply.values[0];
					if ((v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER && v.value._int == 0) || (v.type == Redis::REDIS_RESPONSE_TYPE_STRING && v.value._str.compare("0") == 0)) {
						Redis::Command(mp_redis_connection, 0, "ZREM %s \"%s\"", req->m_for_game.gamename.c_str(), server_key.c_str());
						continue;
					}
				}

				if (request) {
					AppendServerEntry(server_key, &streamed_ret, req->all_keys, false, mp_redis_connection, req);
				}
				else {
					AppendServerEntry(server_key, &ret, req->all_keys, false, mp_redis_connection, req);
				}
			}
			if (request && (!streamed_ret.list.empty() || streamed_ret.last_set)) {
				if (!sent_servers) {
					streamed_ret.first_set = true;
					sent_servers = true;
				}
				//printf("Add server req first: %d last: %d numservs: %d, cursor: %d\n", streamed_ret.first_set, streamed_ret.last_set, streamed_ret.list.size(), cursor);
				request->peer->OnRetrievedServers(*request, streamed_ret, request->extra);
			}

			MM::MMQueryTask::FreeServerListQuery(&streamed_ret);
		} while(cursor != 0);

		error_cleanup:
			return ret;
	}
	ServerListQuery MMQueryTask::GetGroups(const sServerListReq *req, const MMQueryRequest *request) {
		ServerListQuery ret;
		Redis::Response reply;
		Redis::Value v;
		Redis::ArrayValue arr;

		ret.requested_fields = req->field_list;

		Redis::Command(mp_redis_connection, 0, "SELECT %d", OS::ERedisDB_SBGroups);

		int cursor = 0;
		bool sent_servers = false;
		do {
			ServerListQuery streamed_ret;
			reply = Redis::Command(mp_redis_connection, 0, "SCAN %d MATCH %s:*:", cursor, req->m_for_game.gamename.c_str());
			if (Redis::CheckError(reply))
				goto error_cleanup;

			streamed_ret.last_set = false;
			if (cursor == 0) {
				streamed_ret.first_set = true;
			}
			else {
				streamed_ret.first_set = false;
			}

			if (reply.values[0].arr_value.values.size() < 2) {
				goto error_cleanup;
			}

			v = reply.values[0].arr_value.values[0].second;
			arr = reply.values[0].arr_value.values[1].second.arr_value;

			if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
				cursor = atoi(v.value._str.c_str());
			}
			else if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
				cursor = v.value._int;
			}

			if (cursor == 0) {
				streamed_ret.last_set = true;
			}
			for (int i = 0; i<arr.values.size(); i++) {
				if (request) {
					AppendGroupEntry(arr.values[i].second.value._str.c_str(), &streamed_ret, mp_redis_connection, req->all_keys, request);
				}
				else {
					AppendGroupEntry(arr.values[i].second.value._str.c_str(), &ret, mp_redis_connection, req->all_keys, request);
				}				
			}
			if (request && (streamed_ret.last_set || !streamed_ret.list.empty())) {
				if (!sent_servers) {
					streamed_ret.first_set = true;
					sent_servers = true;
				}
				request->peer->OnRetrievedGroups(*request, streamed_ret, request->extra);
			}
			MM::MMQueryTask::FreeServerListQuery(&streamed_ret);
		} while(cursor != 0);

		error_cleanup:
			return ret;
	}
	Server *MMQueryTask::GetServerByKey(std::string key, Redis::Connection *redis_ctx, bool include_deleted) {
		if(redis_ctx == NULL) {
			redis_ctx = mp_redis_connection;
		}
		ServerListQuery ret;
		AppendServerEntry(key, &ret, true, include_deleted, redis_ctx, NULL);
		if(ret.list.size() < 1)
			return NULL;

		return ret.list[0];
	}
	Server *MMQueryTask::GetServerByIP(OS::Address address, OS::GameData game, Redis::Connection *redis_ctx) {
		Server *server = NULL;
		if(redis_ctx == NULL) {
			redis_ctx = mp_redis_connection;
		}
		std::ostringstream s;
		Redis::Response reply;
		Redis::Value v;
		
		Redis::Command(mp_redis_connection, 0, "SELECT %d", OS::ERedisDB_QR);

		s << "GET IPMAP_" << address.ToString(true) << "-" << address.GetPort();
		std::string cmd = s.str();
		reply = Redis::Command(mp_redis_connection, 0, cmd.c_str());
		if (reply.values.size() < 1 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
			goto error_cleanup;

		v = reply.values.front();

		if(v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			server = GetServerByKey(v.value._str, redis_ctx);
		}

		return server;

		error_cleanup:
			if (server)
				delete server;
			return NULL;
	}
	void MMQueryTask::FreeServerListQuery(struct MM::ServerListQuery *query) {
		std::vector<Server *>::iterator it = query->list.begin();
		while(it != query->list.end()) {
			Server *server = *it;
			delete server;
			it++;
		}
	}

	void MMQueryTask::PerformServersQuery(MMQueryRequest request) {
		GetServers(&request.req, &request);
	}
	void MMQueryTask::PerformGroupsQuery(MMQueryRequest request) {
		GetGroups(&request.req, &request); //no need to free, because nothing appended to return value
	}
	void MMQueryTask::PerformGetServerByKey(MMQueryRequest request) {
		ServerListQuery ret;
		AppendServerEntry(request.key, &ret, true, false, NULL, NULL);
		request.peer->OnRetrievedServerInfo(request, ret, request.extra);
		MM::MMQueryTask::FreeServerListQuery(&ret);
	}
	void MMQueryTask::PerformGetServerByIP(MMQueryRequest request) {
		ServerListQuery ret;
		Server *serv = GetServerByIP(request.address, request.SubmitData.game, NULL);
		if(serv)
			ret.list.push_back(serv);

		request.peer->OnRetrievedServerInfo(request, ret, request.extra);

		MM::MMQueryTask::FreeServerListQuery(&ret);
	}
	void MMQueryTask::PerformSubmitData(MMQueryRequest request) {

		std::string src_ip = request.SubmitData.from.ToString(true), dst_ip = request.SubmitData.to.ToString(true);
		Redis::Command(mp_redis_connection, 0, "PUBLISH %s '\\send_msg\\%s\\%s\\%d\\%s\\%d\\%s'",
			sb_mm_channel,
			/*request.SubmitData.game.gamename*/
			"REMOVED",
			src_ip.c_str(),
			request.SubmitData.from.GetPort(),
			dst_ip.c_str(),
			request.SubmitData.to.GetPort(),
			request.SubmitData.base64.c_str());
	}
	void MMQueryTask::PerformGetGameInfoPairByGameName(MMQueryRequest request) {
		OS::GameData games[2];
		if (!m_game_cache->LookupGameByName(request.gamenames[0], games[0])) {
			games[0] = OS::GetGameByName(request.gamenames[0].c_str(), this->mp_redis_connection);
			m_game_cache->AddGame(m_thread_index, games[0]);
		}		
		if (!m_game_cache->LookupGameByName(request.gamenames[1], games[1])) {
			games[1] = OS::GetGameByName(request.gamenames[1].c_str(), this->mp_redis_connection);
			m_game_cache->AddGame(m_thread_index, games[1]);
		}
		
		request.peer->OnRecievedGameInfoPair(games[0], games[1], request.extra);
	}
	void MMQueryTask::PerformGetGameInfoByGameName(MMQueryRequest request) {
		OS::GameData game;
		OS::GameCacheKey key;
		if (!m_game_cache->LookupGameByName(request.gamenames[0], game)) {
			game = OS::GetGameByName(request.gamenames[0].c_str(), this->mp_redis_connection);
			key.gamename = request.gamenames[0];
			key.id = game.gameid;
			m_game_cache->AddItem(m_thread_index, key, game);
		}
		request.peer->OnRecievedGameInfo(game, request.extra);
	}
	void *MMQueryTask::TaskThread(OS::CThread *thread) {
		MMQueryTask *task = (MMQueryTask *)thread->getParams();
		while(thread->isRunning() && (!task->m_request_list.empty() || task->mp_thread_poller->wait()) && thread->isRunning()) {
			task->mp_mutex->lock();
			task->m_thread_awake = true;
			while (!task->m_request_list.empty()) {
				MMQueryRequest task_params = task->m_request_list.front();
				task->mp_mutex->unlock();
				task->mp_timer->start();
				switch (task_params.type) {
				case EMMQueryRequestType_GetServers:
					task->PerformServersQuery(task_params);
					break;
				case EMMQueryRequestType_GetGroups:
					task->PerformGroupsQuery(task_params);
					break;
				case EMMQueryRequestType_GetServerByKey:
					task->PerformGetServerByKey(task_params);
					break;
				case EMMQueryRequestType_GetServerByIP:
					task->PerformGetServerByIP(task_params);
					break;
				case EMMQueryRequestType_SubmitData:
					task->PerformSubmitData(task_params);
					break;
				case EMMQueryRequestType_GetGameInfoByGameName:
					task->PerformGetGameInfoByGameName(task_params);
					break;
				case EMMQueryRequestType_GetGameInfoPairByGameName:
					task->PerformGetGameInfoPairByGameName(task_params);
					break;
				}
				task->mp_timer->stop();
				if(task_params.peer) {
					OS::LogText(OS::ELogLevel_Info, "[%s] Thread type %d - time: %f", task_params.peer->GetSocket()->address.ToString().c_str(), task_params.type, task->mp_timer->time_elapsed()  / 1000000.0);	
				}
				
				task_params.peer->DecRef();
				task->mp_mutex->lock();
				task->m_request_list.pop();
			}
			m_game_cache->timeoutMap(task->m_thread_index);
			task->m_thread_awake = false;
			task->mp_mutex->unlock();
		}
		return NULL;
	}
	void MMQueryTask::debug_dump() {
		mp_mutex->lock();
		printf("Task [%p] awake: %d, num_tasks: %d\n", this, m_thread_awake, m_request_list.size());
		mp_mutex->unlock();
	}
}
