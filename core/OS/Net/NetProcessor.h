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
    //typedef bool (*EncryptionHandler)(OS::Buffer &buffer, O *data, INetPeer *peer);
    //typedef bool (*DecryptionHandler)(OS::Buffer &buffer, O *data, INetPeer *peer);
    typedef bool (*DispatchHandler)(O *data, INetPeer *peer);
    public:
		INetProcessor<O>::INetProcessor() {

		}
		void INetProcessor<O>::SetHandler(DispatchHandler *handler) {
			mp_handler = handler;
		}
		bool INetProcessor<O>::ProcessIncoming(OS::Buffer &buffer, O &output, INetPeer *peer) {
			return deserialize_data(buffer, output);
		}
		bool INetProcessor<O>::SerializeData(O data, OS::Buffer &buffer) {
			return serialize_data(data, buffer);
		}
    protected:
        virtual bool deserialize_data(OS::Buffer &buffer, O &output) = 0;
        virtual bool serialize_data(O &output, OS::Buffer &buffer) = 0;
        virtual bool dispatch_to_peer(O &output, INetPeer *peer) = 0;
        DispatchHandler *mp_handler;
};
#endif //_NETPROCESSOR_H