#ifndef _CHATSERVER_H
#define _CHATSERVER_H
#include <stdint.h>
#include <OS/Net/NetServer.h>

class ChatServer : public INetServer {
public:
	ChatServer();
	~ChatServer();
	void init();
	void tick();
	void shutdown();
	std::string getName() { return m_name; };
private:
	std::string m_name;
	
};
#endif //_CHATSERVER_H