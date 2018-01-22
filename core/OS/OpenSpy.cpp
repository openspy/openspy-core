#include "OpenSpy.h"

#include <sstream>
#include <curl/curl.h>
#include <iomanip>

#include "Auth.h"
#include "Logger.h"

#ifndef _WIN32
	#include "OS/Logger/Unix/UnixLogger.h"
#else
	#include "OS/Logger/Win32/Win32Logger.h"
#endif

#include <OS/legacy/helpers.h>

#include <OS/Auth.h>
#include <OS/Search/User.h>
#include <OS/Search/Profile.h>

namespace OS {
	Logger *g_logger = NULL;
	Config *g_config = NULL;
	struct timeval redis_timeout;
	Redis::Connection *redis_internal_connection = NULL;
	OS::CMutex *mp_redis_internal_connection_mutex = NULL;
	const char *g_appName = NULL;
	const char *g_hostName = NULL;
	const char *g_redisAddress = NULL;
	const char *g_webServicesURL = NULL;

	void Init(const char *appName, const char *configPath) {

		OS::g_config = new Config("openspy.cfg");

		configVar *config_struct = OS::g_config->getRootArray(appName);
		int num_async = OS::g_config->getArrayInt(config_struct, "num_async_tasks");
		const char *hostname = OS::g_config->getArrayString(config_struct, "hostname");
		const char *redis_address = OS::g_config->getArrayString(config_struct, "redis_address");
		const char *webservices_url = OS::g_config->getArrayString(config_struct, "webservices_url");


		g_appName = appName;
		g_hostName = hostname;
		g_webServicesURL = webservices_url;
		g_redisAddress = redis_address;
		curl_global_init(CURL_GLOBAL_SSL);

		redis_timeout.tv_usec = 0;
		redis_timeout.tv_sec = 30;

		redis_internal_connection = Redis::Connect(g_redisAddress, redis_timeout);

		#ifndef _WIN32
			g_logger = new UnixLogger(appName);
		#elif _WIN32
			g_logger = new Win32Logger(appName);
		#endif

		mp_redis_internal_connection_mutex = OS::CreateMutex();
		OS::SetupAuthTaskPool(num_async);
		OS::SetupUserSearchTaskPool(num_async);
		OS::SetupProfileTaskPool(num_async);
		
		OS::LogText(OS::ELogLevel_Info, "%s Init (num async: %d, hostname: %s, redis addr: %s, webservices: %s)\n", appName, num_async, hostname, redis_address, webservices_url);
	}
	void Shutdown() {
		OS::ShutdownAuthTaskPool();
		OS::ShutdownUserSearchTaskPool();
		OS::ShutdownProfileTaskPool();

		Redis::Disconnect(redis_internal_connection);

		delete mp_redis_internal_connection_mutex;
		delete OS::g_config;
		curl_global_cleanup();
	}
	OS::GameData GetGameByRedisKey(const char *key, Redis::Connection *redis_ctx = NULL) {
		GameData game;
		Redis::Response reply;
		Redis::Value v;

		bool must_unlock = false;
		if (redis_ctx == NULL) {
			must_unlock = true;
			redis_ctx = OS::redis_internal_connection;
			mp_redis_internal_connection_mutex->lock();
		}

		Redis::Command(redis_ctx, 0, "SELECT %d", ERedisDB_Game);

		reply = Redis::Command(redis_ctx, 0, "HGET %s gameid", key);
		if (Redis::CheckError(reply)) {
			goto end_error;
		}
		v = reply.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			game.gameid = atoi(OS::strip_quotes(v.value._str).c_str());
		}


		reply = Redis::Command(redis_ctx, 0, "HGET %s secretkey", key);
		if (Redis::CheckError(reply)) {
			goto end_error;
		}
		v = reply.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			strcpy(game.secretkey, OS::strip_quotes(v.value._str).c_str());
		}

		reply = Redis::Command(redis_ctx, 0, "HGET %s description", key);
		if (Redis::CheckError(reply)) {
			goto end_error;
		}
		v = reply.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			strcpy(game.description, OS::strip_quotes(v.value._str).c_str());
		}

		reply = Redis::Command(redis_ctx, 0, "HGET %s gamename", key);
		if (Redis::CheckError(reply)) {
			goto end_error;
		}
		v = reply.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			strcpy(game.gamename, OS::strip_quotes(v.value._str).c_str());
		}

		reply = Redis::Command(redis_ctx, 0, "HGET %s disabled_services", key);
		if (Redis::CheckError(reply)) {
			goto end_error;
		}
		v = reply.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			game.disabled_services = atoi(OS::strip_quotes(v.value._str).c_str());
		}

		reply = Redis::Command(redis_ctx, 0, "HGET %s queryport", key);
		if (Redis::CheckError(reply)) {
			goto end_error;
		}
		v = reply.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			game.queryport = atoi(OS::strip_quotes(v.value._str).c_str());
		}

		reply = Redis::Command(redis_ctx, 0, "HGET %s compatibility_flags", key);
		if (Redis::CheckError(reply)) {
			goto end_error;
		}
		v = reply.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			game.compatibility_flags = atoi(OS::strip_quotes(v.value._str).c_str());
		}
		else {
			game.compatibility_flags = 0;
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

	end_error:
			if (must_unlock) {
				mp_redis_internal_connection_mutex->unlock();
			}
			return game;

	}
	OS::GameData GetGameByName(const char *from_gamename, Redis::Connection *redis_ctx) {
		Redis::Response reply;
		Redis::Value v, arr;

		OS::GameData ret;
		ret.gameid = 0;
		ret.gamename[0] = 0;
		ret.secretkey[0] = 0;
		bool must_unlock = false;
		if(redis_ctx == NULL) {
			must_unlock = true;
			redis_ctx = OS::redis_internal_connection;
			mp_redis_internal_connection_mutex->lock();
		}
		Redis::Command(redis_ctx, 0, "SELECT %d", ERedisDB_Game);
		//memset(&ret, 0, sizeof(ret));
		int cursor = 0;
		do {
			reply = Redis::Command(redis_ctx, 0, "SCAN %d MATCH %s:*", cursor, from_gamename);
			if (Redis::CheckError(reply)) {
				goto end_error;
			}
			v = reply.values[0].arr_value.values[0].second;
		 	if(v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
		 		cursor = atoi(v.value._str.c_str());
		 	} else if(v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
		 		cursor = v.value._int;
		 	}
			arr = reply.values[0].arr_value.values[1].second;
			for(int i=0;i<arr.arr_value.values.size();i++) {
				ret = GetGameByRedisKey(arr.arr_value.values[i].second.value._str.c_str(), redis_ctx);
				break;
			}
		} while(cursor != 0);

	end_error:
		if (must_unlock) {
			mp_redis_internal_connection_mutex->unlock();
		}
		return ret;
	}
	OS::GameData GetGameByID(int gameid, Redis::Connection *redis_ctx) {
		Redis::Response reply;
		Redis::Value v, arr;

		OS::GameData ret;
		bool must_unlock = false;
		if (redis_ctx == NULL) {
			must_unlock = true;
			redis_ctx = OS::redis_internal_connection;
			mp_redis_internal_connection_mutex->lock();
		}
		Redis::Command(redis_ctx, 0, "SELECT %d", ERedisDB_Game);

		int cursor = 0;
		do {
			reply = Redis::Command(redis_ctx, 0, "SCAN %d MATCH *:%d", cursor, gameid);
			if (Redis::CheckError(reply)) {
				goto end_error;
			}
			v = reply.values[0].arr_value.values[0].second;
			if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
				cursor = atoi(v.value._str.c_str());
			}
			else if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
				cursor = v.value._int;
			}
			if (cursor == 0) break;
			arr = reply.values[0].arr_value.values[1].second;

			for (int i = 0; i<arr.arr_value.values.size(); i++) {
				ret = GetGameByRedisKey(arr.arr_value.values[i].second.value._str.c_str(), redis_ctx);
				break;
			}
		} while(cursor != 0);

	end_error:
			if (must_unlock) {
				mp_redis_internal_connection_mutex->unlock();
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
	std::vector<std::string> KeyStringToVector(std::string input) {
		std::vector<std::string> ret;

		std::stringstream ss(input);

		std::string token;

		while (std::getline(ss, token, '\\')) {
			if (!token.length())
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

	std::string strip_whitespace(std::string s) {
		std::string ret;
		std::string::iterator it = s.begin();
		while(it != s.end()) {
			char ch = *(it++);
			if(isspace(ch) && ch != ' ')
				continue;
			ret += ch;
		}
		return ret;
	}
	void Sleep(int time_ms) {
		#ifdef _WIN32
		::Sleep(time_ms);
		#else
		usleep(time_ms * 1000);
		#endif
	}
	Address::Address(uint32_t ip, uint16_t port) {
		this->ip = ip;
		this->port = port;
	}
	Address::Address(struct sockaddr_in addr) {
		ip = addr.sin_addr.s_addr;
		port = addr.sin_port;
	}
	Address::Address(const char *str) {
		char address[16];
		const char *seperator = strrchr(str, ':');
		unsigned int len = strlen(str);
		if(seperator) {
			port = htons(atoi(seperator+1));
			len = seperator - str;
		}
		if(len < sizeof(address)) {
			strncpy(address, str, len);
			address[len] = 0;
		}
		ip = inet_addr((const char *)&address);
	}
	Address::Address() {
		ip = 0;
		port = 0;
	}
	uint16_t Address::GetPort()	{
		return htons(port);
	}
	const struct sockaddr_in Address::GetInAddr() {
		struct sockaddr_in ret;
		ret.sin_family = AF_INET;
		memset(&ret.sin_zero,0,sizeof(ret.sin_zero));
		ret.sin_addr.s_addr = ip;
		ret.sin_port = port;
		return ret;

	}
	std::string Address::ToString(bool ip_only) {
		struct sockaddr_in addr;
		addr.sin_port = (port);
		addr.sin_addr.s_addr = (ip);

		char ipinput[64];
		memset(&ipinput, 0, sizeof(ipinput));

		inet_ntop(AF_INET, &(addr.sin_addr), ipinput, sizeof(ipinput));


		std::ostringstream s;
		s << ipinput;
		if(!ip_only) {
 			s << ":" << htons(port);
		}
		return s.str();
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
	bool wouldBlock() {
		#ifdef _WIN32
		return errno == EWOULDBLOCK;
		#else
		return 0;
		#endif
	}


}
