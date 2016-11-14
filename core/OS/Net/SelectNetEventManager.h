#ifndef _SELECTNETEVENTMGR_H
#define _SELECTNETEVENTMGR_H
#include <OS/OpenSpy.h>
#include "NetEventManager.h"
#include <vector>
class SelectNetEventManager : public INetEventManager {
public:
	SelectNetEventManager();
	~SelectNetEventManager();
	void run();
private:
	int setup_fdset();
	fd_set  m_fdset;
};
#endif //_SELECTNETEVENTMGR_H