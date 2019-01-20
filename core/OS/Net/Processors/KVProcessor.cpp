#include "KVProcessor.h"
KVProcessor::KVProcessor() : INetProcessor<OS::KVReader>() {
    
}

KVProcessor::~KVProcessor() {

}
bool KVProcessor::deserialize_data(OS::Buffer &buffer, OS::KVReader &output) {
        /*
        This scans the incoming packets for \\final\\ and splits based on that,


        as well as handles incomplete packets -- due to TCP preserving data order, this is possible, and cannot be used on UDP protocols
        */
        std::string recv_buf = m_kv_accumulator;
        m_kv_accumulator.clear();
        recv_buf.append((const char *)buffer.GetHead(), buffer.bytesWritten());

        size_t final_pos = 0, last_pos = 0;

        do {
            final_pos = recv_buf.find("\\final\\", last_pos);
            if (final_pos == std::string::npos) break;

            std::string partial_string = recv_buf.substr(last_pos, final_pos - last_pos);
			output = OS::KVReader(partial_string);
            last_pos = final_pos + 7; // 7 = strlen of \\final
        } while (final_pos != std::string::npos);


        //check for extra data that didn't have the final string -- incase of incomplete data
        if (last_pos < (size_t)buffer.bytesWritten()) {
            std::string remaining_str = recv_buf.substr(last_pos);
            m_kv_accumulator.append(remaining_str);
        }

        if (m_kv_accumulator.length() > MAX_UNPROCESSED_DATA) {
            //Delete();
            return false;
        }
		return true;
}
bool KVProcessor::serialize_data(OS::KVReader &output, OS::Buffer &buffer) {
	std::string s = output.ToString();
	return true;
}
bool KVProcessor::dispatch_to_peer(OS::KVReader &output, INetPeer *peer) {
	return true;
}