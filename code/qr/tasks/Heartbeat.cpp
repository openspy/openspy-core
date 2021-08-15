
#include <tasks/tasks.h>
#include <sstream>
#include "../server/v2.h"
#include <server/QRDriver.h>
#include <OS/gamespy/gsmsalg.h>

namespace MM {
    std::string GetServerKeyBy_InstanceKey_Address(TaskThreadData *thread_data, uint32_t instance_key, OS::Address address) {
        std::ostringstream s;
        s << "IPINSTMAP_" << instance_key << "-" << address.ToString(true) << "-" << address.GetPort();

		Redis::Response reply;
		Redis::Value v;

        std::string server_key = "";


        Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_QR);
		reply = Redis::Command(thread_data->mp_redis_connection, 0, "GET %s", s.str().c_str());
		if (Redis::CheckError(reply)) {
			return server_key;
		}
		v = reply.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			server_key = OS::strip_quotes(v.value._str).c_str();
		}
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
    std::string GenerateChallenge(OS::GameData game_data) {
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

        return challenge;
    }
    std::string GetNewServerKey_FromRequest(MMPushRequest request, TaskThreadData *thread_data, std::string gamename, int &server_id) {
        return GetNewServerKey(thread_data, request.v2_instance_key, request.server.m_address, gamename, server_id);
    }
    std::string Get_QR1_Stored_Address_ServerKey(MMPushRequest request, TaskThreadData *thread_data) {
        std::ostringstream s;
        s << "QR1MAP_" << request.from_address.ToString(true) << "-" << request.from_address.GetPort();

		Redis::Response reply;
		Redis::Value v;

        Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_QR);
		reply = Redis::Command(thread_data->mp_redis_connection, 0, "GET %s", s.str().c_str());
		if (Redis::CheckError(reply)) {
			return "";
		}
		v = reply.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			return OS::strip_quotes(v.value._str).c_str();
		}
        return "";
    }
    std::string GetServerKey_FromRequest(MMPushRequest request, TaskThreadData *thread_data) {
        std::string key;
        bool use_stored_qr1_address = false;
        if(request.version == 1) {            
            //qr1, do "query lookup" for non-data incoming queries
            switch(request.type) {
                //use from address
                case EMMPushRequestType_Heartbeat:
                break;

                //use stored QR1 address
                case EMMPushRequestType_ValidateServer:
                case EMMPushRequestType_Keepalive:
                default:
                use_stored_qr1_address = true;
                break;
            }

            if(use_stored_qr1_address) {
                key = Get_QR1_Stored_Address_ServerKey(request, thread_data);
            }
        }
        if(!use_stored_qr1_address || key.length() == 0)
            key = GetServerKeyBy_InstanceKey_Address(thread_data, request.v2_instance_key, request.from_address);
        return key;
    }
    bool PerformHeartbeat(MMPushRequest request, TaskThreadData *thread_data) {
        MMTaskResponse response;
        response.v2_instance_key = request.v2_instance_key;
        response.driver = request.driver;
        response.from_address = request.from_address;
        response.query_address = request.server.m_address;

        Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_QR);

        int statechanged = 1;
        if(request.server.m_keys.find("statechanged") != request.server.m_keys.end()) {
            std::string statechanged_val = request.server.m_keys["statechanged"];
            statechanged = atoi(statechanged_val.c_str());
            
        }

        //check for server by instance key + ip:port
        std::string server_key = GetServerKey_FromRequest(request, thread_data);
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
            if(game_info.secretkey.length() == 0) {
                //return adderror
                response.error_message = "No such gamename";
                request.callback(response);
                return true;
            }

            int server_id;
            server_key = GetNewServerKey_FromRequest(request, thread_data, gamename, server_id);

            std::string challenge_string = GenerateChallenge(game_info);

            char challenge_resp[90] = { 0 };
            gsseckey((unsigned char *)&challenge_resp, challenge_string.c_str(), (const unsigned char *)game_info.secretkey.c_str(), 0);

            if(request.version == 1) {
                const int V1_MAX_CHALLENGE_LEN = 8;
                challenge_resp[V1_MAX_CHALLENGE_LEN] = 0;
                challenge_string = challenge_string.substr(0, V1_MAX_CHALLENGE_LEN);
            }

            if(request.type == EMMPushRequestType_Heartbeat_ClearExistingKeys) {
                ClearServerCustKeys(thread_data, server_key);
            }
            WriteServerData(thread_data, server_key, request.server, request.v2_instance_key, request.from_address);
            SetServerDeleted(thread_data, server_key, 1);
            SetServerInitialInfo(thread_data, request.driver->getListenerSocket()->address, server_key, game_info, challenge_resp, request.server.m_address, server_id, request.from_address);

            response.challenge = challenge_string;
            request.callback(response);
        } else {
            if(statechanged == 2) {
                SetServerDeleted(thread_data, server_key, 1);

                std::ostringstream s;
                s << "\\del\\" << server_key.c_str();
                thread_data->mp_mqconnection->sendMessage(mm_channel_exchange, mm_server_event_routingkey, s.str());

                request.callback(response);
                return;
            }
            if(request.type == EMMPushRequestType_Heartbeat_ClearExistingKeys) {
                ClearServerCustKeys(thread_data, server_key);
            }
            WriteServerData(thread_data, server_key, request.server, request.v2_instance_key, request.from_address);

            std::ostringstream s;
            s << "\\update\\" << server_key.c_str();

            if(!(server_key.length() > 7 && server_key.substr(0, 7).compare("thugpro") == 0)) { //temporarily supress thugpro updates
                thread_data->mp_mqconnection->sendMessage(mm_channel_exchange, mm_server_event_routingkey, s.str());
            }
            
            
            request.callback(response);
        }
        return true;
    }
    void WriteLastHeartbeatTime(TaskThreadData *thread_data, std::string server_key, OS::Address address, uint32_t instance_key, OS::Address from_address) {
        struct timeval current_time;
        gettimeofday(&current_time, NULL);
        Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s last_heartbeat %d", server_key.c_str(), current_time.tv_sec);

        if(instance_key == 0 && address.GetPort() != from_address.GetPort()) { //instance key is 0, likely QR1
            Redis::Command(thread_data->mp_redis_connection, 0, "EXPIRE QR1MAP_%s-%d %d", from_address.ToString(true).c_str(), from_address.GetPort(), MM_PUSH_EXPIRE_TIME);
        }
        
        Redis::Command(thread_data->mp_redis_connection, 0, "EXPIRE IPMAP_%s-%d %d", address.ToString(true).c_str(), address.GetPort(), MM_PUSH_EXPIRE_TIME);
        Redis::Command(thread_data->mp_redis_connection, 0, "EXPIRE IPINSTMAP_%ld-%s-%d %d", instance_key, address.ToString(true).c_str(), address.GetPort(), MM_PUSH_EXPIRE_TIME);
        Redis::Command(thread_data->mp_redis_connection, 0, "EXPIRE %s %d", server_key.c_str(), MM_PUSH_EXPIRE_TIME);
        Redis::Command(thread_data->mp_redis_connection, 0, "EXPIRE %scustkeys %d", server_key.c_str(), MM_PUSH_EXPIRE_TIME);
    }
    void WriteServerData(TaskThreadData *thread_data, std::string server_key, ServerInfo server, uint32_t instance_key, OS::Address from_address) {
        std::string ipinput = server.m_address.ToString(true);

		Redis::Command(thread_data->mp_redis_connection, 0, "SET IPMAP_%s-%d %s", ipinput.c_str(), server.m_address.GetPort(), server_key.c_str());

        if(instance_key == 0 && server.m_address.GetPort() != from_address.GetPort()) {
            Redis::Command(thread_data->mp_redis_connection, 0, "SET QR1MAP_%s-%d %s", from_address.ToString(true).c_str(), from_address.GetPort(), server_key.c_str());
        }
            

        Redis::Command(thread_data->mp_redis_connection, 0, "SET IPINSTMAP_%ld-%s-%d %s", instance_key, ipinput.c_str(), server.m_address.GetPort(), server_key.c_str());
        Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s instance_key %ld", server_key.c_str(), instance_key);

		Redis::Command(thread_data->mp_redis_connection, 0, "HINCRBY %s num_updates 1", server_key.c_str());

		std::map<std::string, std::string>::iterator it = server.m_keys.begin();
		while (it != server.m_keys.end()) {
			std::pair<std::string, std::string> p = *it;
			Redis::Command(thread_data->mp_redis_connection, 0, "HSET %scustkeys %s \"%s\"", server_key.c_str(), p.first.c_str(), OS::escapeJSON(p.second).c_str());
			it++;
		}
		

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
					Redis::Command(thread_data->mp_redis_connection, 0, "HSET %scustkeys_player_%d %s \"%s\"", server_key.c_str(), i, p.first.c_str(), OS::escapeJSON(s).c_str());
				}
				i++;
				it3++;
			}
			max_idx= i;
			i = 0;
			it2++;
		}

		for(i=0;i<max_idx;i++) {
			Redis::Command(thread_data->mp_redis_connection, 0, "EXPIRE %scustkeys_player_%d %d", server_key.c_str(), i, MM_PUSH_EXPIRE_TIME);
		}
		i=0;


		it2 = server.m_team_keys.begin();
		while (it2 != server.m_team_keys.end()) {
			p = *it2;
			it3 = p.second.begin();
			while (it3 != p.second.end()) {

				std::string s = *it3;
				if(s.length() > 0) {
					Redis::Command(thread_data->mp_redis_connection, 0, "HSET %scustkeys_team_%d %s \"%s\"", server_key.c_str(), i, p.first.c_str(), OS::escapeJSON(s).c_str());
					i++;
				}
				it3++;
			}
			max_idx= i;
			i = 0;
			it2++;
		}
		for(i=0;i<max_idx;i++) {
			Redis::Command(thread_data->mp_redis_connection, 0, "EXPIRE %scustkeys_team_%d %d", server_key.c_str(), i, MM_PUSH_EXPIRE_TIME);
		}
		i=0;

		WriteLastHeartbeatTime(thread_data, server_key, server.m_address, instance_key, from_address);

    }
	void SetServerDeleted(TaskThreadData *thread_data, std::string server_key, bool deleted) {
        Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s deleted %d", server_key.c_str(), deleted);
    }
	void SetServerInitialInfo(TaskThreadData *thread_data, OS::Address driver_address, std::string server_key, OS::GameData game_info, std::string challenge_response, OS::Address address, int id, OS::Address from_address) {
        Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s driver_address %s", server_key.c_str(), driver_address.ToString().c_str());
        Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s driver_hostname %s", server_key.c_str(), OS::g_hostName);
        Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s qr_port %d", server_key.c_str(), from_address.GetPort());
        Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s wan_port %d", server_key.c_str(), address.GetPort());
        Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s wan_ip \"%s\"", server_key.c_str(), address.ToString(true).c_str());
        Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s gameid %d", server_key.c_str(), game_info.gameid);
        Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s id %d", server_key.c_str(), id);

        Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s challenge %s", server_key.c_str(), challenge_response.c_str());

        Redis::Command(thread_data->mp_redis_connection, 0, "ZADD %s %d \"%s\"", game_info.gamename.c_str(), id, server_key.c_str());
    }
    void ClearServerCustKeys(TaskThreadData *thread_data, std::string server_key) {
		Redis::Response reply;

		std::string key;
		Redis::Value v, arr;

		int cursor = 0;
        int channel_id;

        do {
            reply = Redis::Command(thread_data->mp_redis_connection, 0, "SCAN %d MATCH %scustkeys*", cursor, server_key.c_str());
            if (Redis::CheckError(reply))
                return;

            v = reply.values[0].arr_value.values[0].second;
            arr = reply.values[0].arr_value.values[1].second;

			if (arr.type == Redis::REDIS_RESPONSE_TYPE_ARRAY) {

				if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
					cursor = atoi(v.value._str.c_str());
				}
				else if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
					cursor = v.value._int;
				}
				for (size_t i = 0; i < arr.arr_value.values.size(); i++) {
					if (arr.arr_value.values[i].first != Redis::REDIS_RESPONSE_TYPE_STRING)
						continue;

					key = arr.arr_value.values[i].second.value._str;
                    Redis::Command(thread_data->mp_redis_connection, 0, "DEL %s", key.c_str());
                }
            }
        } while(cursor != 0);
        
    }
}