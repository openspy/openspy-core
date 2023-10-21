#ifndef _SMSERVER_H
#define _SMSERVER_H
#include <stdint.h>
#include <OS/Net/NetServer.h>
namespace MM {
	class UTMasterRequest;
}
namespace UT {
	class Server : public INetServer {
	public:
		Server();
		virtual ~Server();
		void tick();
		void shutdown();
        void doInternalLoadGameData(redisContext *ctx); //called by async task on startup
		private:
	};
}
#endif //_SMSERVER_H
