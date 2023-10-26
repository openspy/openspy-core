#include "tasks.h"
#include <sstream>
#include <server/Server.h>

#define CHANNEL_EXPIRE_TIME 300
namespace Peerchat {
    UserSummary GetUserSummaryByName(TaskThreadData *thread_data, std::string name) {
        std::string formatted_name;
        std::transform(name.begin(),name.end(),std::back_inserter(formatted_name),tolower);

        redisReply *reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "EXISTS usernick_%s", formatted_name.c_str());

        bool exists = false;
        if(reply) {
           exists = reply->integer;
           freeReplyObject(reply);
        }

        if(!exists) {
            return UserSummary();
        }


        reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "GET usernick_%s", formatted_name.c_str());
        
        int id = 0;

        if(reply) {
            if(reply->type == REDIS_REPLY_STRING) {
                id = atoi(reply->str);
            } else if(reply->type == REDIS_REPLY_INTEGER) {
                id = reply->integer;
            }
            freeReplyObject(reply);
        }
        return LookupUserById(thread_data, id);
		
    }
    UserSummary LookupUserById(TaskThreadData *thread_data, int user_id) {
        UserSummary summary;
        if (user_id == -1) {
            return *server_userSummary;
        }

        std::ostringstream ss;
        ss << "user_" << user_id;

        std::string user_key = ss.str();

        redisReply *reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "EXISTS %s", user_key.c_str());

        bool exists = false;
        if(reply) {
           exists = reply->integer;
           freeReplyObject(reply);
        }

        if(!exists) {
             summary.id = 0;
             return summary;
        }

        //reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "HMGET %s username nick realname hostname address modeflags operflags profileid userid gameid");

		const char *args[] = {
			"HMGET", user_key.c_str(), "username", "nick", "realname", "hostname", "address", "modeflags", "operflags", "profileid", "userid", "gameid"
		};

        reply = (redisReply *)redisCommandArgv(thread_data->mp_redis_connection, sizeof(args) / sizeof(const char *), args, NULL);

        if(reply) {
            if(reply->elements == 10) {
                if(reply->element[0]->type == REDIS_REPLY_STRING) {
                    summary.username = reply->element[0]->str;
                }

                if(reply->element[1]->type == REDIS_REPLY_STRING) {
                    summary.nick = reply->element[1]->str;
                }

                if(reply->element[2]->type == REDIS_REPLY_STRING) {
                    summary.realname = reply->element[2]->str;
                }


                if(reply->element[3]->type == REDIS_REPLY_STRING) {
                    summary.hostname = reply->element[3]->str;
                }

                if(reply->element[4]->type == REDIS_REPLY_STRING) {
                    summary.address = OS::Address(reply->element[4]->str);
                }

                if(reply->element[5]->type == REDIS_REPLY_STRING) {
                    summary.modeflags = atoi(reply->element[5]->str);
                }

                if(reply->element[6]->type == REDIS_REPLY_STRING) {
                    summary.operflags = atoi(reply->element[6]->str);
                }

                if(reply->element[7]->type == REDIS_REPLY_STRING) {
                    summary.profileid = atoi(reply->element[7]->str);
                }

                if(reply->element[8]->type == REDIS_REPLY_STRING) {
                    summary.userid = atoi(reply->element[8]->str);
                }

                if(reply->element[9]->type == REDIS_REPLY_STRING) {
                    summary.gameid = atoi(reply->element[9]->str);
                }
            }

            freeReplyObject(reply);
        }

        summary.id = user_id;

        return summary;
    }
    int CountServerUsers(TaskThreadData* thread_data) {
        int count = 0;
        redisReply *reply;
        int cursor = 0;
		std::vector<ChannelSummary> channels;
		ChannelSummary summary;
		
        do {
            reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "SCAN %d MATCH usernick_*", cursor);
			if (reply == NULL)
				break;

		 	if(reply->element[0]->type == REDIS_REPLY_STRING) {
		 		cursor = atoi(reply->element[0]->str);
		 	} else if (reply->element[0]->type == REDIS_REPLY_INTEGER) {
		 		cursor = reply->element[0]->integer;
		 	}

            count += reply->element[1]->elements;
            freeReplyObject(reply);

        } while(cursor != 0);
        return count;
    }
}