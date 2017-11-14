#ifndef _SMSERVER_H
#define _SMSERVER_H
#include <stdint.h>
#include <OS/Net/NetServer.h>
#define SM_SERVER_PORT 29901
namespace SM {
	class Server : public INetServer {
	public:
		Server();
		void init();
		void tick();
		void shutdown();
		OS::MetricInstance GetMetrics();
	private:
		struct timeval m_last_analytics_submit_time;
	};
}
#endif //_SMSERVER_H