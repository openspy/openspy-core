
#include <tasks/tasks.h>
#include <sstream>
#include "../server/v2.h"
#include <server/QRDriver.h>
#include <OS/gamespy/gsmsalg.h>

namespace MM {
	std::string GetServerKeyBy_InstanceKey_Address(TaskThreadData *thread_data, uint32_t instance_key, OS::Address address) {
		std::ostringstream s;
		s << "IPINSTMAP_" << instance_key << "-" << address.ToString(true) << "-" << address.GetPort();

		redisReply *reply;
		

		std::string server_key = "";
		
		std::string ipmap_str = s.str();

		redisAppendCommand(thread_data->mp_redis_connection, "GET %s", ipmap_str.c_str());
		
		int r = redisGetReply(thread_data->mp_redis_connection,(void**)&reply);
		if(r != REDIS_OK) {
			return "";
		}
		
		if(reply->type == REDIS_REPLY_STRING) {
			server_key = reply->str;
		}
		freeReplyObject(reply);

		if(server_key.length() != 0) {
			if(isServerDeleted(thread_data, server_key)) {
				return "";
			}
		}
		return server_key;
	}
	std::string GetNewServerKey(TaskThreadData *thread_data, uint32_t instance_key, OS::Address address, std::string gamename, int &id) {
		id = TryFindServerID(thread_data, address);

		if(id == -1)
			id = GetServerID(thread_data);

		std::ostringstream s;
		s << gamename << ":" << id << ":";
		std::string server_key = s.str();

		return server_key;
	}
	std::string GenerateChallenge(OS::GameData game_data, OS::Address from_address) {
		char challenge[V2_CHALLENGE_LEN + 1];
		memset(&challenge, 0, sizeof(challenge));
		OS::gen_random((char *)&challenge,sizeof(challenge)-1);

		uint8_t *backend_flags = (uint8_t *)&challenge[6];

		//force this part of the challenge to match sscanf pattern %02x, by setting the most significant bit		
		char hex_chars[] = "0123456789abcdef";
		if(game_data.backendflags & QR2_OPTION_USE_QUERY_CHALLENGE) {
			*backend_flags = hex_chars[(rand() % 8) + 8];
		} else if(!(game_data.backendflags & QR2_OPTION_USE_QUERY_CHALLENGE)) {
			*backend_flags = hex_chars[rand() % 8];
		}
		*(backend_flags+1) = hex_chars[rand() % (sizeof(hex_chars)-1)];

		char *ip_string = (char *)&challenge[8];
		snprintf(ip_string, sizeof(challenge) - 8,"%08X%04X", htonl(from_address.GetIP()), from_address.GetPort());

		return challenge;
	}
	std::string GetNewServerKey_FromRequest(MMPushRequest request, TaskThreadData *thread_data, std::string gamename, int &server_id) {
		return GetNewServerKey(thread_data, request.v2_instance_key, request.server.m_address, gamename, server_id);
	}
	std::string Get_QR1_Stored_Address_ServerKey(MMPushRequest request, TaskThreadData *thread_data) {
		std::string server_key;

		std::ostringstream s;
		s << "QR1MAP_" << request.from_address.ToString(true) << "-" << request.from_address.GetPort();

		redisReply *reply;

		redisAppendCommand(thread_data->mp_redis_connection, "SELECT %d", OS::ERedisDB_QR);
		
		std::string qrmap_key = s.str();
		redisAppendCommand(thread_data->mp_redis_connection, "GET %s", qrmap_key.c_str());

		int r = redisGetReply(thread_data->mp_redis_connection,(void**)&reply);		
		if(r == REDIS_OK) {
			freeReplyObject(reply);  
		}

		r = redisGetReply(thread_data->mp_redis_connection,(void**)&reply);		

		if(r != REDIS_OK) {
			return "";
		}

		if(reply->type == REDIS_REPLY_STRING) {
			server_key = reply->str;
		}
		freeReplyObject(reply);
	
		return OS::strip_quotes(server_key.c_str());
	}
	std::string GetServerKey_FromRequest(MMPushRequest request, TaskThreadData *thread_data) {
		std::string key;
		if(request.version == 1) {            
			key = Get_QR1_Stored_Address_ServerKey(request, thread_data);
		}
		if(key.length() == 0) {
			key = GetServerKeyBy_InstanceKey_Address(thread_data, request.v2_instance_key, request.from_address);
		}
		return key;
	}
	bool CheckServerKey_HasRecentHeartbeat_V1(MMPushRequest request, TaskThreadData* thread_data, std::string server_key) {
		redisReply* reply;

		if (server_key.length() < 1) {
			return false;
		}

		std::string hbtime_key = server_key + "_hbblock";
		bool keyExists = false;
		reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "GET %s", hbtime_key.c_str());
		
		if(!reply) {
			return false;
		}
		if (reply->type == REDIS_REPLY_INTEGER) {
			if (reply->integer >= MM_QRV1_NUM_BURST_REQS)
				keyExists = true;
		}
		else if (reply->type == REDIS_REPLY_STRING) {
			if (atoi(reply->str) >= MM_QRV1_NUM_BURST_REQS)
				keyExists = true;
		}
		
		freeReplyObject(reply);

		//add key to block future  requests
		if (!keyExists) {
			redisAppendCommand(thread_data->mp_redis_connection, "INCR %s", hbtime_key.c_str());
			redisAppendCommand(thread_data->mp_redis_connection, "EXPIRE %s %d", hbtime_key.c_str(), MM_IGNORE_TIME_SECS);

			int r = redisGetReply(thread_data->mp_redis_connection,(void**)&reply);		
			if(r == REDIS_OK) {
				freeReplyObject(reply);  
			}

			r = redisGetReply(thread_data->mp_redis_connection,(void**)&reply);		
			if(r == REDIS_OK) {
				freeReplyObject(reply);  
			}
		}
		return keyExists; //if a key is created, that means the request can be processed (future ones will be blocked)
	}
	bool CheckServerKey_HasRecentHeartbeat(MMPushRequest request, TaskThreadData* thread_data, std::string server_key) {
		#if 1
		return false;
		#else
		redisReply* reply;
		if (request.version == 1) {
			return CheckServerKey_HasRecentHeartbeat_V1(request, thread_data, server_key);
		}

		if (server_key.length() < 1) {
			return false;
		}

		std::string hbtime_key = server_key + "_hbblock";
		bool keyExists = false;
		reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "EXISTS %s", hbtime_key.c_str());
	
		if(!reply) {
			return false;
		}

		if (reply->type == REDIS_REPLY_INTEGER) {
			if (reply->integer != 0)
				keyExists = true;
		}

		freeReplyObject(reply);
		
		//add key to block future  requests
		if (!keyExists) {
			redisAppendCommand(thread_data->mp_redis_connection, "SET %s 1", hbtime_key.c_str());
			redisAppendCommand(thread_data->mp_redis_connection, "EXPIRE %s %d", hbtime_key.c_str(), MM_IGNORE_TIME_SECS);

			int r = redisGetReply(thread_data->mp_redis_connection,(void**)&reply);		
			if(r == REDIS_OK) {
				freeReplyObject(reply);  
			}

			r = redisGetReply(thread_data->mp_redis_connection,(void**)&reply);		
			if(r == REDIS_OK) {
				freeReplyObject(reply);  
			}

		}
		return keyExists; //if a key is created, that means the request can be processed (future ones will be blocked)
		#endif
	}
	bool PerformHeartbeat(MMPushRequest request, TaskThreadData *thread_data) {
		MMTaskResponse response;
		response.v2_instance_key = request.v2_instance_key;
		response.driver = request.driver;
		response.from_address = request.from_address;
		response.query_address = request.server.m_address;

		int statechanged = 1;
		if(request.server.m_keys.find("statechanged") != request.server.m_keys.end()) {
			std::string statechanged_val = request.server.m_keys["statechanged"];
			statechanged = atoi(statechanged_val.c_str());
			
		}

		//check for server by instance key + ip:port
		std::string server_key = GetServerKey_FromRequest(request, thread_data);

		if (statechanged == 1) {
			//This is a transparent error (the server will not return an error) - 
			// this is because the throttling does not mean the server will removed / not registered - it just means this heartbeat is ignored
			// the error response means to the client the server is no longer registered
			if (CheckServerKey_HasRecentHeartbeat(request, thread_data, server_key)) {
				OS::LogText(OS::ELogLevel_Info, "[%s] Rejecting request due to recent heartbeat - %s", request.from_address.ToString().c_str(), server_key.c_str());
				request.callback(response);
				return true;
			}
			else {
				OS::LogText(OS::ELogLevel_Info, "[%s] Accepting heartbeat - %s", request.from_address.ToString().c_str(), server_key.c_str());
			}
		}

		//if not exists, state changed 3 (which means QR V2 has not been registered yet), or QR1 server is "fully deleted", meaning it could be a resume, or was not registered yet (secure challenge dropped)
		if(server_key.length() == 0 || statechanged == 3 || (request.version == 1 && isServerDeleted(thread_data, server_key, true))) {
			if(request.server.m_keys.find("gamename") == request.server.m_keys.end()) {
				//return adderror
				response.error_message = "No gamename supplied";
				request.callback(response);
				return true;
			}


			std::string gamename = request.server.m_keys["gamename"];
			OS::GameData game_info = OS::GetGameByName(gamename.c_str(), thread_data->mp_redis_connection);
			selectQRRedisDB(thread_data);
			if(game_info.secretkey.length() == 0) {
				//return adderror
				response.error_message = "No such gamename";
				request.callback(response);
				return true;
			}

			int server_id;
			server_key = GetNewServerKey_FromRequest(request, thread_data, gamename, server_id);

			std::string challenge_string = GenerateChallenge(game_info, request.from_address);

			char challenge_resp[90] = { 0 };
			gsseckey((unsigned char *)&challenge_resp, challenge_string.c_str(), (const unsigned char *)game_info.secretkey.c_str(), 0);

			if(request.version == 1) {
				const int V1_MAX_CHALLENGE_LEN = 8; //some especially old UT games like unreal will ignore anything beyond this length
				challenge_resp[V1_MAX_CHALLENGE_LEN] = 0;
				challenge_string = challenge_string.substr(0, V1_MAX_CHALLENGE_LEN);
			}

			if(request.type == EMMPushRequestType_Heartbeat_ClearExistingKeys) {
				ClearServerCustKeys(thread_data, server_key);
			}
			WriteServerData(thread_data, server_key, request.server, request.v2_instance_key, request.from_address);
			SetServerDeleted(thread_data, server_key, 1, request.version != 1);
			SetServerInitialInfo(thread_data, request.driver->GetAddress(), server_key, game_info, challenge_resp, request.server.m_address, server_id, request.from_address);

			response.challenge = challenge_string;
			request.callback(response);
		} else {
			if(statechanged == 2) {
				SetServerDeleted(thread_data, server_key, 1);

				std::ostringstream s;
				s << "\\del\\" << server_key.c_str();
				
				std::string msg = s.str();
				TaskShared::sendAMQPMessage(mm_channel_exchange, mm_server_event_routingkey, msg.c_str(), &request.from_address);

				request.callback(response);
				return true;
			}
			if(request.type == EMMPushRequestType_Heartbeat_ClearExistingKeys) {
				ClearServerCustKeys(thread_data, server_key);
			}

			std::ostringstream s;

			int hb_count = IncrNumHeartbeats(thread_data, server_key);

			if(hb_count == 1 && request.version == 1) { //fire V1 new event, which only occurs on first HB, instead of validation success
				s << "\\new\\" << server_key.c_str();
			} else {
				s << "\\update\\" << server_key.c_str();
			}

			WriteServerData(thread_data, server_key, request.server, request.v2_instance_key, request.from_address);
		

			std::string msg = s.str();
			TaskShared::sendAMQPMessage(mm_channel_exchange, mm_server_event_routingkey, msg.c_str(), &request.from_address);
			
			request.callback(response);
		}
		return true;
	}
	void WriteLastHeartbeatTime(TaskThreadData *thread_data, std::string server_key, OS::Address address, uint32_t instance_key, OS::Address from_address) {
		int count = 0;
		uv_timespec64_t current_time;
		uv_clock_gettime(UV_CLOCK_REALTIME, &current_time);
		redisAppendCommand(thread_data->mp_redis_connection, "HSET %s last_heartbeat %d", server_key.c_str(), current_time.tv_sec); count++;

		std::ostringstream s;

		if(instance_key == 0) { //instance key is 0, likely QR1
			s << "QR1MAP_" << address.ToString(true) << "-" << address.GetPort();
			std::string qr1map_key = s.str();
			s.str("");
			redisAppendCommand(thread_data->mp_redis_connection, "EXPIRE %s %d", qr1map_key.c_str(), MM_PUSH_EXPIRE_TIME); count++;

			if(address.GetPort() != from_address.GetPort()) {
				s << "QR1MAP_" << from_address.ToString(true) << "-" << from_address.GetPort();
				qr1map_key = s.str();
				s.str("");
				redisAppendCommand(thread_data->mp_redis_connection, "EXPIRE %s %d", qr1map_key.c_str(), MM_PUSH_EXPIRE_TIME); count++;
			}
		}
		std::string server_key_custkeys = server_key + "custkeys";
		
		
		s << "IPINSTMAP_" << instance_key << "-" << address.ToString(true) << "-" << address.GetPort();
		std::string ipinstmap_str = s.str();
		s.str("");

		s << "IPMAP_" << address.ToString(true) << "-" << address.GetPort();
		std::string ipmap_str = s.str();

		redisAppendCommand(thread_data->mp_redis_connection, "EXPIRE %s %d", ipmap_str.c_str(), MM_PUSH_EXPIRE_TIME); count++;
		redisAppendCommand(thread_data->mp_redis_connection, "EXPIRE %s %d", ipinstmap_str.c_str(), MM_PUSH_EXPIRE_TIME); count++;
		redisAppendCommand(thread_data->mp_redis_connection, "EXPIRE %s %d", server_key.c_str(), MM_PUSH_EXPIRE_TIME); count++;
		redisAppendCommand(thread_data->mp_redis_connection, "EXPIRE %s %d", server_key_custkeys.c_str(), MM_PUSH_EXPIRE_TIME); count++;

		void *reply;
		for(int i =0;i<count;i++) {
			int r = redisGetReply(thread_data->mp_redis_connection,(void**)&reply);		
			if(r == REDIS_OK) {
				freeReplyObject(reply);
			}
		}        
	}
	void WriteServerData(TaskThreadData *thread_data, std::string server_key, ServerInfo server, uint32_t instance_key, OS::Address from_address) {
		int total_redis_calls = 0;
		std::string ipinput = server.m_address.ToString(true);

		std::ostringstream s;
		s << "IPINSTMAP_" << instance_key << "-" << server.m_address.ToString(true) << "-" << server.m_address.GetPort();
		std::string ipinstmap_str = s.str();
		s.str("");

		s << "IPMAP_" << server.m_address.ToString(true) << "-" << server.m_address.GetPort();
		std::string ipmap_str = s.str();
		s.str("");

		redisAppendCommand(thread_data->mp_redis_connection, "SET %s %s", ipmap_str.c_str(), server_key.c_str()); total_redis_calls++;

		if(instance_key == 0) {
			s << "QR1MAP_" << server.m_address.ToString(true) << "-" << server.m_address.GetPort();
			std::string qr1map_key = s.str();
			s.str("");
			redisAppendCommand(thread_data->mp_redis_connection, "SET %s %s", qr1map_key.c_str(), server_key.c_str()); total_redis_calls++;

			if(server.m_address.GetPort() != from_address.GetPort()) {
				s << "QR1MAP_" << from_address.ToString(true) << "-" << from_address.GetPort();
				qr1map_key = s.str();
				s.str("");
				redisAppendCommand(thread_data->mp_redis_connection, "SET %s %s", qr1map_key.c_str(), server_key.c_str()); total_redis_calls++;
			}
		} 

		redisAppendCommand(thread_data->mp_redis_connection, "SET %s %s", ipinstmap_str.c_str(), server_key.c_str()); total_redis_calls++;
		redisAppendCommand(thread_data->mp_redis_connection, "HSET %s instance_key %ld", server_key.c_str(), instance_key); total_redis_calls++;

		//redisAppendCommand(thread_data->mp_redis_connection, "HINCRBY %s num_updates 1", server_key.c_str()); total_redis_calls++;
	
		if (!server.m_keys.empty()) {
			std::string custkeys_key = server_key + "custkeys";
			
			size_t num_keys = 2 + (server.m_keys.size() * 2);
			
			const char **args = (const char **)malloc(num_keys * sizeof(const char *));
			
			//HMSET is being deprecated, but currently the server needs it... just only involve changing HMSET to HSET (but keeping same syntax) when change is needed
			args[0] = "HMSET";
			args[1] = custkeys_key.c_str();
			
			int idx = 2;

			std::map<std::string, std::string>::iterator it = server.m_keys.begin();
			while (it != server.m_keys.end()) {
				args[idx++] = it->first.c_str();
				args[idx++] = it->second.c_str();
				it++;
			}
			
			redisAppendCommandArgv(thread_data->mp_redis_connection, num_keys, args, NULL); total_redis_calls++;
			free(args);
		}



		// This map stored the keys remapped in a way which is easier to write to redis (so we can iterate each hash to write at a time instead of key by key)
		std::map<std::string, std::vector<std::pair<std::string, std::string>>> transposed_keys;
		std::map<std::string, std::vector<std::string> >::iterator it2 = server.m_player_keys.begin();
		std::vector<std::string>::iterator it3;
		std::pair<std::string, std::vector<std::string> > p;
		int i = 0;

		//Map team keys to transposd keys
		while (it2 != server.m_player_keys.end()) {
			p = *it2;
			it3 = p.second.begin();
			while (it3 != p.second.end()) {
				std::string key = p.first;
				std::string value = *it3;
				if (value.length() > 0) {
					std::ostringstream player_ss;
					player_ss << server_key << "custkeys_player_" << i;
					transposed_keys[player_ss.str()].push_back(std::pair<std::string, std::string>(key, value));
				}
				i++;
				it3++;
			}

			i = 0;
			it2++;
		}

		//now do the same thing for team keys
		it2 = server.m_team_keys.begin();
		while (it2 != server.m_team_keys.end()) {
			p = *it2;
			it3 = p.second.begin();
			while (it3 != p.second.end()) {
				std::string key = p.first;
				std::string value = *it3;
				if (value.length() > 0) {
					std::ostringstream player_ss;
					player_ss << server_key << "custkeys_team_" << i;
					transposed_keys[player_ss.str()].push_back(std::pair<std::string, std::string>(key, value));
				}
				i++;
				it3++;
			}

			i = 0;
			it2++;
		}

		//now actually write to redis
		std::map<std::string, std::vector<std::pair<std::string, std::string>>>::iterator transposed_keys_it = transposed_keys.begin();
		while (transposed_keys_it != transposed_keys.end()) {
			auto p = *transposed_keys_it;
			std::vector<std::pair<std::string, std::string>>::iterator keys_it = p.second.begin();
			
			if(!p.second.empty()) {
				int num_keys = 2 + (p.second.size() * 2);
				
				const char **args = (const char **)malloc(num_keys * sizeof(const char *));
				
				args[0] = "HMSET";
				args[1] = p.first.c_str();
				
				int idx = 2;

				while (keys_it != p.second.end()) {
					if (!keys_it->second.empty()) {
						args[idx++] = keys_it->first.c_str();
						args[idx++] = keys_it->second.c_str();
					}
					keys_it++;
				}

				redisAppendCommandArgv(thread_data->mp_redis_connection, idx, args, NULL); total_redis_calls++;
				redisAppendCommand(thread_data->mp_redis_connection, "EXPIRE %s %d", p.first.c_str(), MM_PUSH_EXPIRE_TIME); total_redis_calls++;
				free(args);
			}

			transposed_keys_it++;
		}

		redisReply *reply;
		for(int i =0;i<total_redis_calls;i++) {
			int r = redisGetReply(thread_data->mp_redis_connection,(void**)&reply);
			if(r == REDIS_OK) {
				freeReplyObject(reply);  
			}
		}
		WriteLastHeartbeatTime(thread_data, server_key, server.m_address, instance_key, from_address);

	}
	void SetServerDeleted(TaskThreadData *thread_data, std::string server_key, bool deleted, bool reset_hbcount) {
		void *reply;
		redisAppendCommand(thread_data->mp_redis_connection, "HSET %s deleted %d", server_key.c_str(), deleted);

		if(reset_hbcount) { //typically we don't want to reset hbcount in QR1 because some games will request a new 
			redisAppendCommand(thread_data->mp_redis_connection, "HDEL %s num_updates", server_key.c_str());
		}		

		int r = redisGetReply(thread_data->mp_redis_connection,(void**)&reply);
		if(r == REDIS_OK) {
			freeReplyObject(reply);  
		}

		if(reset_hbcount) {
			r = redisGetReply(thread_data->mp_redis_connection,(void**)&reply);		
			if(r == REDIS_OK) {
				freeReplyObject(reply);  
			}
		}
	}
	void SetServerInitialInfo(TaskThreadData *thread_data, OS::Address driver_address, std::string server_key, OS::GameData game_info, std::string challenge_response, OS::Address address, int id, OS::Address from_address) {
		
		std::ostringstream ss;
		
		std::string qrport_str, wanport_str, wanip_str, gameid_str, id_str;
		
		ss << from_address.GetPort();
		qrport_str = ss.str();
		ss.str("");
		
		ss << address.GetPort();
		wanport_str = ss.str();
		ss.str("");
		
		ss << address.ToString(true);
		wanip_str = ss.str();
		ss.str("");
		
		ss << game_info.gameid;
		gameid_str = ss.str();
		ss.str("");
		
		ss << id;
		id_str = ss.str();
		ss.str("");
		
		std::string driver_address_str = driver_address.ToString();
		const char *keys[] = {
			"HMSET", server_key.c_str(),
			"driver_address", driver_address_str.c_str(),
			"driver_hostname", OS::g_hostName,
			"qr_port", qrport_str.c_str(),
			"wan_port", wanport_str.c_str(),
			"wan_ip", wanip_str.c_str(),
			"gameid", gameid_str.c_str(),
			"id", id_str.c_str(),
			"challenge", challenge_response.c_str()
		};
		
		int x = sizeof(keys) / sizeof(const char *);
		
		redisAppendCommandArgv(thread_data->mp_redis_connection, x, keys, NULL);
		redisAppendCommand(thread_data->mp_redis_connection, "ZADD %s %d %s", game_info.gamename.c_str(), id, server_key.c_str());

		void *reply;
		int r = redisGetReply(thread_data->mp_redis_connection, &reply);
		if(r == REDIS_OK) {
			freeReplyObject(reply);
		}
		
		r = redisGetReply(thread_data->mp_redis_connection, &reply);
		if(r == REDIS_OK) {
			freeReplyObject(reply);
		}
	}
	void ClearServerCustKeys(TaskThreadData *thread_data, std::string server_key) {
		int cursor = 0;
		redisReply *reply;
		std::string cust_keys_match = server_key + "custkeys*";
		do {
			reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "SCAN %d MATCH %s", cursor, cust_keys_match.c_str());
			if (reply == NULL || thread_data->mp_redis_connection->err) {
				goto error_cleanup;
			}
			if (reply->elements < 2) {
				goto error_cleanup;
			}

			if(reply->element[0]->type == REDIS_REPLY_STRING) {
				cursor = atoi(reply->element[0]->str);
			} else if (reply->element[0]->type == REDIS_REPLY_INTEGER) {
				cursor = reply->element[0]->integer;
			}

			//issue delete cmds
			for(size_t i=0;i<reply->element[1]->elements;i += 2) {
				std::string key = reply->element[1]->element[i]->str;
				redisAppendCommand(thread_data->mp_redis_connection, "DEL %s", key.c_str());
			}

			//now free resources
			for(size_t i=0;i<reply->element[1]->elements;i += 2) {
				void *sub_reply;
				int r = redisGetReply(thread_data->mp_redis_connection,(void**)&sub_reply);		
				if(r == REDIS_OK) {
					freeReplyObject(sub_reply);  
				}
			}
		} while(cursor != 0);

		reply = NULL;

		error_cleanup:
		if(reply != NULL) {
			freeReplyObject(reply);
		}     
	}
}
