#include "OpenSpy.h"

#include <sstream>
#include <curl/curl.h>
#include <iomanip>

#include "Logger.h"

#ifndef _WIN32
	#include "OS/Logger/Unix/UnixLogger.h"
#else
	#include "OS/Logger/Win32/Win32Logger.h"
#endif

#include <OS/Config/AppConfig.h>

#include <uv.h>

namespace OS {
	Logger *g_logger = NULL;
	AppConfig *g_config = NULL;
	const char *g_appName = NULL;
	const char *g_hostName = NULL;
	const char *g_redisAddress = NULL;
	const char *g_webServicesURL = NULL;
	const char *g_webServicesAPIKey = NULL;
	const char *g_redisUsername = NULL;
	const char *g_redisPassword = NULL;
	int			g_numAsync = 0;
	CURL *g_curl = NULL;
	CURLSH *g_curlShare = NULL;
	CMutex **g_curlMutexes= NULL;

	void get_server_address_port(const char *input, char *address, uint16_t &port) {
		const char *seperator = strrchr(input, ':');
		size_t len = strlen(input);
		if (seperator) {
			port = atoi(seperator + 1);
			len = seperator - input;
		}
		strncpy(address, input, len);
		address[len] = 0;
	}

	void Init(const char *appName, AppConfig *appConfig) {
		//SSL_library_init();
		curl_global_init(CURL_GLOBAL_SSL);

		OS::g_config = appConfig;
		OS::g_curl = curl_easy_init(); //only used for curl_easy_escape

		g_curlShare = curl_share_init();
		g_curlMutexes = (OS::CMutex **)malloc(sizeof(CMutex *) * CURL_LOCK_DATA_LAST);
		for(int i=0;i<CURL_LOCK_DATA_LAST;i++) {
			g_curlMutexes[i] = OS::CreateMutex();
		}

		curl_share_setopt(g_curlShare, CURLSHOPT_LOCKFUNC, curlLockCallback);
		curl_share_setopt(g_curlShare, CURLSHOPT_UNLOCKFUNC, curlUnlockCallback);
		curl_share_setopt(g_curlShare, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);

		g_appName = appName;

		char temp_env_buffer[256];
		size_t temp_env_sz = sizeof(temp_env_buffer);

		uv_os_getenv("OPENSPY_HOSTNAME", (char *)&temp_env_buffer, &temp_env_sz);
		g_hostName = strdup(temp_env_buffer);

		temp_env_sz = sizeof(temp_env_buffer);
		uv_os_getenv("OPENSPY_WEBSERVICES_URL", (char *)&temp_env_buffer, &temp_env_sz);
		g_webServicesURL = strdup(temp_env_buffer);

		temp_env_sz = sizeof(temp_env_buffer);
		uv_os_getenv("OPENSPY_API_KEY", (char *)&temp_env_buffer, &temp_env_sz);
		g_webServicesAPIKey = strdup(temp_env_buffer);

		temp_env_sz = sizeof(temp_env_buffer);
		uv_os_getenv("OPENSPY_REDIS_ADDRESS", (char *)&temp_env_buffer, &temp_env_sz);
		g_redisAddress = strdup(temp_env_buffer);

		if(strlen(g_redisAddress) > 0) {
			temp_env_sz = sizeof(temp_env_buffer);
			uv_os_getenv("OPENSPY_REDIS_USER", (char *)&temp_env_buffer, &temp_env_sz);
			g_redisUsername = strdup(temp_env_buffer);
		}
			
		if(strlen(g_redisAddress) > 0) {
			temp_env_sz = sizeof(temp_env_buffer);
			uv_os_getenv("OPENSPY_REDIS_PASSWORD", (char *)&temp_env_buffer, &temp_env_sz);
			g_redisPassword = strdup(temp_env_buffer);
		}

		#ifndef _WIN32
			g_logger = new UnixLogger(appName);
		#elif _WIN32
			g_logger = new Win32Logger(appName);
		#endif

		OS::LogText(OS::ELogLevel_Info, "%s Init (num async: %d, hostname: %s, redis addr: %s, redis creds: %s, webservices: %s)\n", appName, g_numAsync, g_hostName, g_redisAddress, g_redisUsername, g_webServicesURL);
	}
	void Shutdown() {
		delete g_logger;
		delete OS::g_config;
		curl_easy_cleanup(OS::g_curl);
		curl_share_cleanup(OS::g_curlShare);
		curl_global_cleanup();

		for(int i=0;i<CURL_LOCK_DATA_LAST;i++) {
			delete g_curlMutexes[i];
		}

		free((void *)g_hostName);
		free((void *)g_webServicesURL);
		free((void *)g_webServicesAPIKey);
		free((void *)g_redisAddress);
		if(g_redisUsername != NULL) {
			free((void *)g_redisUsername);
		}
		if(g_redisPassword != NULL) {
			free((void *)g_redisPassword);
		}
		free((void *)g_curlMutexes);
	}
	OS::GameData GetGameByRedisKey(const char *key, redisContext *redis_ctx) {
		GameData game;
		redisReply *reply;

		bool must_unlock = false;

		redisAppendCommand(redis_ctx, "SELECT %d", ERedisDB_Game);
		redisAppendCommand(redis_ctx, "HMGET %s gameid secretkey description gamename disabled_services queryport backendflags", key);

		int r = redisGetReply(redis_ctx,(void**)&reply);
        if(r == REDIS_OK) {
            freeReplyObject(reply);
            
            r = redisGetReply(redis_ctx,(void**)&reply);
            
            if(r == REDIS_OK) {
                
                int idx = 0;
                if(reply->element[idx]->type == REDIS_REPLY_STRING) {
                    game.gameid = atoi(reply->element[idx]->str);
                }
                idx++;
                
                if(reply->element[idx]->type == REDIS_REPLY_STRING) {
                    game.secretkey = reply->element[idx]->str;
                }
                idx++;
                
                if(reply->element[idx]->type == REDIS_REPLY_STRING) {
                    game.description = reply->element[idx]->str;
                }
                idx++;
                
                if(reply->element[idx]->type == REDIS_REPLY_STRING) {
                    game.gamename = reply->element[idx]->str;
                }
                idx++;
                
                if(reply->element[idx]->type == REDIS_REPLY_STRING) {
                    game.disabled_services = atoi(reply->element[idx]->str);
                }
                idx++;
                
                if(reply->element[idx]->type == REDIS_REPLY_STRING) {
                    game.queryport = atoi(reply->element[idx]->str);
                }
                idx++;
                
                if(reply->element[idx]->type == REDIS_REPLY_STRING) {
                    game.backendflags = atoi(reply->element[idx]->str);
                } else {
                    game.backendflags = 0;
                }
                idx++;
                
                freeReplyObject(reply);
            }
        }


		/*
		//for thugpro
		game.popular_values.push_back("SKATOPIA");
		game.popular_values.push_back("NEW ORLEANS");
		game.popular_values.push_back("LOS ANGELES");
		game.popular_values.push_back("SCHOOL");
		game.popular_values.push_back("Graffiti");

		//thugpro test
		game.push_keys["country"] = KEYTYPE_STRING;
		game.push_keys["gamemode"] = KEYTYPE_STRING;
		game.push_keys["gametype"] = KEYTYPE_STRING;
		game.push_keys["gamever"] = KEYTYPE_STRING;
		game.push_keys["hostname"] = KEYTYPE_STRING;
		game.push_keys["mapname"] = KEYTYPE_STRING;
		game.push_keys["maxplayers"] = KEYTYPE_BYTE;
		game.push_keys["numplayers"] = KEYTYPE_BYTE;
		game.push_keys["password"] = KEYTYPE_BYTE;
		*/

		return game;

	}
	OS::GameData GetGameByName(const char *from_gamename, redisContext *redis_ctx) {
		redisReply *reply;

		OS::GameData game_data;

		bool must_unlock = false;

		redisAppendCommand(redis_ctx, "SELECT %d", ERedisDB_Game);
		redisAppendCommand(redis_ctx, "GET %s", from_gamename);

		int r = redisGetReply(redis_ctx,(void**)&reply);
        
        if(r == REDIS_OK) {
            freeReplyObject(reply);
        }

		r = redisGetReply(redis_ctx,(void**)&reply);

		if(reply && r == REDIS_OK) {
			if(reply->type == REDIS_REPLY_STRING) {
				game_data = OS::GetGameByRedisKey(reply->str, redis_ctx);
			}			
			freeReplyObject(reply);
		}

		return game_data;
	}
	OS::GameData GetGameByID(int gameid, redisContext *redis_ctx) {
		redisReply* reply;

		OS::GameData ret;

		redisAppendCommand(redis_ctx, "SELECT %d", ERedisDB_Game);
		redisAppendCommand(redis_ctx, "GET gameid_%d",gameid);

		int r = redisGetReply(redis_ctx,(void**)&reply);
        if(r == REDIS_OK) {
            freeReplyObject(reply);
        }

		r = redisGetReply(redis_ctx,(void**)&reply);
		if(r == REDIS_OK) {
            if(reply->type == REDIS_REPLY_STRING) {
                ret = GetGameByRedisKey(reply->str, redis_ctx);
            }
            freeReplyObject(reply);
		}
		return ret;
	}
	std::map<std::string, std::string> KeyStringToMap(std::string input) {
		std::map<std::string, std::string> ret;

		std::stringstream ss(input);

		std::string key, token;

		int i = 0;
		while (std::getline(ss, token, '\\')) {
			if (!token.length()) {
				i++;
				continue;
			}

			if(i % 2) {
				key = token;
			} else {
				ret[key] = token;
			}

			i++;
		}
		return ret;

	}
	
	std::vector<std::string> KeyStringToVector(std::string input, bool skip_null, char delimator) {
		if(skip_null)
			input = input.substr(1);

		std::vector<std::string> ret;

		std::stringstream ss(input);

		std::string token;

		while (std::getline(ss, token, delimator)) {
			if (!token.length() && !skip_null)
				continue;
			ret.push_back(token);
		}
		return ret;

	}
	std::string MapToKVString(std::map<std::string, std::string> kv_data) {
		std::ostringstream s;
		std::map<std::string, std::string>::iterator it = kv_data.begin();
		while(it != kv_data.end()) {
			std::pair<std::string, std::string> p = *it;
			s << "\\" << p.first << "\\" << p.second;
			it++;
		}
		return s.str();
	}

	std::vector< std::map<std::string, std::string> > ValueStringToMapArray(std::vector<std::string> fields, std::string values) {
		std::vector< std::map<std::string, std::string> > ret;
		values = values.substr(1);
		std::stringstream ss(values);
		std::string token;
		std::map<std::string, std::string> temp_map;
		int field_count = fields.size();

		int idx = 0;

		bool skip_next = false; //might not be desired, as it is specific to the weird legacy SB protocol KV strings

		while (std::getline(ss, token, '\\')) {
			if(skip_next) {
				skip_next = false;
				continue;
			}

			if(idx % field_count == 0 && idx != 0) {
				idx = 0;
				ret.push_back(temp_map);
				temp_map.clear();
				skip_next = true;
			}
		}


		return ret;
	}


	CThread *CreateThread(OS::ThreadEntry *entry, void *param,  bool auto_start) {
		#if _WIN32
			return new OS::CWin32Thread(entry, param, auto_start);
		#else
			return new OS::CPThread(entry, param, auto_start);
		#endif
	}
	CMutex *CreateMutex() {
		#if _WIN32
			return new OS::CWin32Mutex();
		#else
			return new OS::CPMutex();
		#endif
	}
	CThreadPoller *CreateThreadPoller() {
		#if _WIN32
		return new OS::CWin32ThreadPoller();
		#else
		return new OS::CPThreadPoller();
		#endif
	}
	std::string strip_quotes(std::string s) {
		if(s[0] != '"' || s[s.length()-1] != '"')
			return s;
		return s.substr(1, s.size() - 2);
	}
	
	std::string escapeJSON(const std::string& input)
	{
		std::string output;
		output.reserve(input.length());

		for (std::string::size_type i = 0; i < input.length(); ++i)
		{
			switch (input[i]) {
			case '"':
				output += "\\\"";
				break;
			case '/':
				output += "\\/";
				break;
			case '\b':
				output += "\\b";
				break;
			case '\f':
				output += "\\f";
				break;
			case '\n':
				output += "\\n";
				break;
			case '\r':
				output += "\\r";
				break;
			case '\t':
				output += "\\t";
				break;
			case '\\':
				output += "\\\\";
				break;
			default:
				output += input[i];
				break;
			}

		}

		return output;
	}

	std::string unescapeJSON(const std::string& input)
	{
		bool escaped = false;
		std::string output;
		output.reserve(input.length());

		for (std::string::size_type i = 0; i < input.length(); ++i)
		{
			if(escaped) {
				switch (input[i])
				{
				case '"':
					output += '\"';
					break;
				case '/':
					output += '/';
					break;
				case 'b':
					output += '\b';
					break;
				case 'f':
					output += '\f';
					break;
				case 'n':
					output += '\n';
					break;
				case 'r':
					output += '\r';
					break;
				case 't':
					output += '\t';
					break;
				case '\\':
					output += '\\';
					break;
				default:
					output += input[i];
					break;
				}

				escaped = false;
				break;
			}
			else 
			{
				switch (input[i])
				{
				case '\\':
					escaped = true;
					break;
				default:
					output += input[i];
					break;
				}
			}
		}
		return output;
	}

	std::string strip_whitespace(std::string s, bool skip_spaces) {
		std::ostringstream ss;
		std::string::iterator it = s.begin();
		while(it != s.end()) {
			unsigned char ch = *(it++);
			if(isspace(ch) && (ch != ' ' || (skip_spaces && ch == ' ')))
				continue;
			ss << ((char)ch);
		}
		return ss.str();
	}
	void Sleep(int time_ms) {
		#ifdef _WIN32
		::Sleep(time_ms);
		#else
		sleep(time_ms / 1000);
		#endif
	}

	template<typename Out>
	void split(const std::string &s, char delim, Out result) {
	    std::stringstream ss;
	    ss.str(s);
	    std::string item;
	    while (std::getline(ss, item, delim)) {
	        *(result++) = item;
	    }
	}


	std::vector<std::string> split(const std::string &s, char delim) {
	    std::vector<std::string> elems;
	    split(s, delim, std::back_inserter(elems));
	    return elems;
	}
	void LogText(ELogLevel level, const char *fmt, ...) {
		va_list args;
		va_start (args, fmt);
		if (g_logger) {
			g_logger->LogText(level, fmt, args);
		}
		
		va_end(args);
	}

	std::string FindBestMatch(std::vector<std::string> matches, std::string name) {
		std::vector<std::string>::iterator it = matches.begin();
		int best_score = 0, match_result;
		std::string best_result;
		while(it != matches.end()) {
			std::string s = *it;
			if(match2(s.c_str(),name.c_str(), match_result) == 0) {
				if(match_result > best_score) {
					best_result = s;
					best_score = match_result;
				}
			}
			it++;
		}
		return best_result;
	}
	std::string url_encode(std::string src) {
		char *ret = curl_easy_escape(OS::g_curl, src.c_str(), (int)src.length());
		std::string ret_str = ret;

		curl_free(ret);
		return ret_str;
	}
	std::string url_decode(std::string src) {
		char *ret = curl_easy_unescape(OS::g_curl, src.c_str(), (int)src.length(), NULL);
		std::string ret_str = ret;
		curl_free(ret);
		return ret_str;
	}

	void curlLockCallback(CURL *handle, curl_lock_data data, curl_lock_access access, void *userptr) {
		g_curlMutexes[data]->lock();
	}
	void curlUnlockCallback(CURL *handle, curl_lock_data data, void *userptr) {
		g_curlMutexes[data]->unlock();
	}


	int match2(const char *mask, const char *name, int &match_count, char wildcard_char)
	{
		const u_char *m = (u_char *)mask;
		const u_char *n = (u_char *)name;
		const u_char *ma = NULL;
		const u_char *na = (u_char *)name;

		match_count = 0;

		while (1)
		{
			if (*m == wildcard_char)
			{
				while (*m == wildcard_char) /* collapse.. */
					m++;
				ma = m;
				na = n;
			}

			if (!*m)
			{
				if (!*n)
					return 0;
				if (!ma)
					return 1;
				for (m--; (m > (const u_char *)mask) && (*m == '?'); m--);
				if (*m == wildcard_char)
					return 0;
				m = ma;
				n = ++na;
			}
			else
				if (!*n)
				{
					while (*m == wildcard_char) /* collapse.. */
						m++;
					return (*m != 0);
				}

			if ((tolower(*m) != tolower(*n)) && !((*m == '_') && (*n == ' ')) && (*m != '?'))
			{
				if (!ma)
					return 1;
				m = ma;
				n = ++na;
			}
			else
			{
				m++;
				n++;
				match_count++;
			}
		}
		return 1;
	}
	int match(const char *mask, const char *name) {
		int match_count;
		if (mask[0] == '*' && mask[1] == '!') {
			mask += 2;
			while (*name != '!' && *name)
				name++;
			if (!*name)
				return 1;
			name++;
		}

		if (mask[0] == '*' && mask[1] == '@') {
			mask += 2;
			while (*name != '@' && *name)
				name++;
			if (!*name)
				return 1;
			name++;
		}
		return match2(mask, name, match_count);
	}
	void gen_random(char *s, const int len, int time_multiplier) {
		int i;
		srand((unsigned int)time(NULL) * time_multiplier);
		static const char alphanum[] =
			"0123456789"
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			"abcdefghijklmnopqrstuvwxyz";

		for (i = 0; i < len; ++i) {
			s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
		}

		s[len] = 0;
	}
}
