#ifndef _SBSERVER_H
#define _SBSERVER_H
#include <stdint.h>
#include <OS/Net/NetServer.h>

class SBServer : public INetServer {
public:
	SBServer();
	~SBServer();
	void init();
	void tick();
	void shutdown();

private:
	
};
#endif //_CHCGAMESERVER_H