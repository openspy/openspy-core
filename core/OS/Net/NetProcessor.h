/*
    Generic application protocol dispatcher 
*/
#ifndef _NETPROCESSOR_H
#define _NETPROCESSOR_H
#include <OS/OpenSpy.h>
#include <OS/Net/NetPeer.h>
#include <OS/Net/NetDriver.h>
template<typename O>
class INetProcessor {
    public:
		INetProcessor<O>() {

		}
		virtual ~INetProcessor<O>() {
			
		}
		bool ProcessIncoming(OS::Buffer &buffer, std::vector<O> &output) {
			return deserialize_data(buffer, output);
		}
		bool SerializeData(O data, OS::Buffer &buffer) {
			return serialize_data(data, buffer);
		}
    protected:
        virtual bool deserialize_data(OS::Buffer &buffer, std::vector<O> &output) = 0;
        virtual bool serialize_data(O &output, OS::Buffer &buffer) = 0;
};
#endif //_NETPROCESSOR_H