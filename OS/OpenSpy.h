#ifndef _OPENSPY_H
#define _OPENSPY_H
#ifdef _WIN32
#include <Windows.h>
#include <WinSock.h>
typedef int socklen_t;
#define snprintf sprintf_s
#define strcasecmp _stricmp
#define strncasecmp _strnicmp

int gettimeofday(struct timeval *tp, struct timezone *tzp);
#else
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/times.h>

#endif
#include <string>
#include <vector>
#include <stdint.h>
#include <memory.h>
#include <hiredis/hiredis.h>
namespace OS {

	///////////////////////
	/// XXX: put in os/geo/region.h
	typedef struct {
		const char *countrycode;
		const char *countryname;
		int region;
	} countryRegion;
	///////////////////
	//// XXX: put in os/game.h
	typedef struct {
		int gameid;
		int queryport;
		char gamename[32];
		char description[64];
		char secretkey[7];
		char disabled_services; //0= ok, 1 = temp, 2 = perm
	} GameData;
	GameData GetGameByName(const char *from_gamename);
	GameData GetGameByID(int gameid);
	enum ERedisDB {
		ERedisDB_QR,
		ERedisDB_SBGroups,
		ERedisDB_Game,
	};
	
	void Init();
	extern redisContext *redis_connection;
	std::vector<std::string> KeyStringToMap(std::string input);
}



#endif //_OPENSPY_H
