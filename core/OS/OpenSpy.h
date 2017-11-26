#ifndef _OPENSPY_H
#define _OPENSPY_H

#include <stdio.h>

#ifdef _WIN32
#include <Windows.h>
#include <WinSock.h>
#include "Threads/Win32/WinThread.h"
#include "Threads/Win32/Win32Mutex.h"
#include "Threads/Win32/Win32ThreadPoller.h"
#define EVTMGR_USE_SELECT 1
//#define EVTMGR_USE_EPOLL 0
typedef int socklen_t;
#ifndef snprintf
	#define snprintf sprintf_s
#endif
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define MSG_NOSIGNAL 0
#define close closesocket
int gettimeofday(struct timeval *tp, struct timezone *tzp);
#else
//#define EVTMGR_USE_SELECT 0
#define EVTMGR_USE_EPOLL 1
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/times.h>
#include "Threads/PThreads/PThread.h"
#include "Threads/PThreads/PMutex.h"
#include "Threads/PThreads/PThreadPoller.h"

#define stricmp strcasecmp
#define sprintf_s snprintf
#define strnicmp strncasecmp
#define vsprintf_s vsnprintf
#define _strnicmp strnicmp

#endif
#include <stdlib.h>
#include <stdarg.h>
#include <string>
#include <vector>
#include <stdint.h>
#include <memory.h>
#include <map>
#include <OS/Redis.h>

#include <OS/Logger.h>

//#define OPENSPY_WEBSERVICES_URL "http://os-auth.us-east-1.elasticbeanstalk.com"
#define OPENSPY_WEBSERVICES_URL "http://10.1.1.141:8000"
#define OS_REDIS_SERV "opensly-redis.u95v0m.0001.use1.cache.amazonaws.com"
#define OS_REDIS_PORT 6379
//#define OS_REDIS_ADDR "opensly-redis.u95v0m.0001.use1.cache.amazonaws.com:6379"
#define OS_REDIS_ADDR "10.1.1.125:6379"

namespace OS {
	class Logger;
	extern Logger *g_logger;
	extern const char *g_appName;
	extern const char *g_hostName;
	void LogText(ELogLevel level, const char *fmt, ...);
	///////////////////////
	/// XXX: put in os/geo/region.h
	typedef struct {
		const char *countrycode;
		const char *countryname;
		int region;
	} countryRegion;
	///////////////////
	//// XXX: put in os/game.h

	//key types for the key type list
	#define KEYTYPE_STRING	0
	#define KEYTYPE_BYTE	1
	#define KEYTYPE_SHORT	2
	#define KEYTYPE_INT		3


	#define OS_MAX_GAMENAME 32
	#define OS_MAX_DESCRIPTION 64
	#define OS_MAX_SECRETKEY 7

	#define OS_GAMESPY_PARTNER_CODE 0
	#define OS_IGN_PARTNER_CODE 10
	#define OS_EA_PARTNER_CODE 20
	
	typedef enum
	{
		QR2_GAME_AVAILABLE,
		QR2_GAME_UNAVAILABLE,
		QR2_GAME_TEMPORARILY_UNAVAILABLE,
	} QRV2AvailableStatus;

	#define OS_COMPATIBILITY_FLAG_SBV2_FROMGAME_LIST_NOKEYS 1
	
	typedef struct {
		int gameid;
		int queryport;
		char gamename[OS_MAX_GAMENAME];
		char description[OS_MAX_DESCRIPTION];
		char secretkey[OS_MAX_SECRETKEY];
		char disabled_services; //0= ok, 1 = temp, 2 = perm
		int compatibility_flags;
		std::vector<std::string> popular_values;
		std::map<std::string, uint8_t> push_keys; //SB push keys + type(hostname/KEYTYPE_STRING)
	} GameData;

	GameData GetGameByName(const char *from_gamename, Redis::Connection *redis_ctx = NULL);
	GameData GetGameByID(int gameid, Redis::Connection *redis_ctx = NULL);
	enum ERedisDB {
		ERedisDB_QR,
		ERedisDB_SBGroups,
		ERedisDB_Game,
		ERedisDB_NatNeg,
		ERedisDB_Chat,
	};


	class Address {
	public:
		Address(struct sockaddr_in);
		Address(const char *str);
		Address();
		Address(uint32_t ip, uint16_t port);
		uint32_t GetIP() { return ip; };
		uint16_t GetPort();
		const struct sockaddr_in GetInAddr();
		std::string ToString(bool ip_only = false);
	//private:
		uint32_t ip;
		uint16_t port;
	};

	template<typename Out>
	void split(const std::string &s, char delim, Out result);
	std::vector<std::string> split(const std::string &s, char delim);

	void Init(const char *appName, int num_async_tasks, const char *hostName);
	void Shutdown();
	std::map<std::string, std::string> KeyStringToMap(std::string input);
	std::vector<std::string> KeyStringToVector(std::string input);
	std::string MapToKVString(std::map<std::string, std::string> kv_data);
	std::string strip_quotes(std::string s);
	std::string strip_whitespace(std::string s);

	std::string escapeJSON(const std::string& input);
	std::string unescapeJSON(const std::string& input);

	#define MAX_BASE64_STR 768
	void Base64StrToBin(const char *str, uint8_t **out, int &len);
	const char *BinToBase64Str(const uint8_t *in, int in_len);

	const char *MD5String(const char *string);

	//thread
	CThread *CreateThread(ThreadEntry *entry, void *param, bool auto_start);
	CMutex *CreateMutex();
	CThreadPoller *CreateThreadPoller();

	void Sleep(int time_ms);

	std::string FindBestMatch(std::vector<std::string> matches, std::string name);

	bool wouldBlock();

}

#ifdef _WIN32
const char* inet_ntop(int af, const void *src, char *dst, size_t size);
#endif



#endif //_OPENSPY_H
