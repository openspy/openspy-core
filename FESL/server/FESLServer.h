#ifndef _SMSERVER_H
#define _SMSERVER_H
#include <stdint.h>
#include <OS/Net/NetServer.h>
#include <OS/Net/IOIfaces/SSLIOInterface.h>
#define FESL_SERVER_PORT 18300

namespace FESL {
	class Server : public INetServer {
	public:
		Server();
		void init();
		void tick();
		void shutdown();
		OS::MetricInstance GetMetrics();
		SSLNetIOIFace::SSLNetIOInterface *getSSLIO_Interface();
	private:
		struct timeval m_last_analytics_submit_time;
		SSLNetIOIFace::SSLNetIOInterface *mp_ssl_io_interface;
	};
}
#endif //_SMSERVER_H