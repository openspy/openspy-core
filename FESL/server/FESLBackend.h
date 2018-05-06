#ifndef _FESLBACKEND_H
#define _FESLBACKEND_H
#include "main.h"

#include <OS/OpenSpy.h>
#include <OS/Task.h>
#include <OS/TaskPool.h>
#include <OS/Thread.h>
#include <OS/Mutex.h>
#include <OS/Redis.h>
#include <vector>
#include <map>
#include <string>

#include <OS/Timer/HiResTimer.h>

#include "FESLServer.h"


namespace FESLBackend {
	void onRedisMessage(Redis::Connection *c, Redis::Response reply, void *privdata);
    void SetupFESLBackend(FESL::Server *server);
    void ShutdownFESLBackend();

};

#endif //_FESLBACKEND_H