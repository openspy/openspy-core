#ifndef _KVPROCESSOR_H
#define _KVPROCESSOR_H
#include <OS/KVReader.h>
#include "../NetProcessor.h"
#define MAX_UNPROCESSED_DATA 5000
class KVProcessor : public INetProcessor<OS::KVReader> {
    public:
        KVProcessor();
        ~KVProcessor();
    protected:
		bool deserialize_data(OS::Buffer &buffer, OS::KVReader &output);
		bool serialize_data(OS::KVReader &output, OS::Buffer &buffer);
		bool dispatch_to_peer(OS::KVReader &output, INetPeer *peer);

    private:
        std::string m_kv_accumulator;
};
#endif //_KVPROCESSOR_H