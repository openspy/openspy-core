#ifndef _SBSERVER_H
#define _SBSERVER_H
#include <stdint.h>
#include <OS/Net/NetServer.h>
#include <tasks/tasks.h>

class SBServer : public INetServer {
public:
	SBServer();
	~SBServer();
	void init();
	void tick();

	void OnNewServer(MM::Server server);
	void OnUpdateServer(MM::Server server);
	void OnDeleteServer(MM::Server server);
private:
};
#endif //_SBSERVER_H