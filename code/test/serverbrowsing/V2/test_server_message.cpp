#include "common.h"

#include <OS/Redis.h>

#include "server/SBServer.h"
#include "server/SBDriver.h"

namespace MM {
    extern const char *sb_mm_channel;
};

OS::CThread *mp_async_thread;
Redis::Connection *mp_servermsg_redis_conn;

const char *test_message = "\xFD\xFC\x1E\x66\x6A\xB2\x11\x11\x11\x11";
int test_message_size = 10;

void Send_FakeServerMsgReq(SBTestState *test_state, OS::Address dest, char *buf, int buff_len) {
	OS::Buffer buff;
	buff.WriteShort(htons(777)); //length
	buff.WriteByte(SEND_MESSAGE_REQUEST); //request type
	buff.WriteInt(dest.GetIP());
	buff.WriteShort(dest.GetPort());
	buff.WriteBuffer(buf, buff_len);
	buff.resetReadCursor();
	test_netio_iface->fake_PushIncomingPacket(test_state->peer->GetSocket(), buff);
	test_state->peer->think(true);
}
bool Fake_SendCallback(INetIOSocket *socket, OS::Buffer buffer) {
	return false;
}

void onRedisMessage(Redis::Connection *c, Redis::Response reply, void *privdata) {
    Redis::Value v = reply.values.front();
	std::string gamename, from_ip, to_ip, from_port, to_port, data, type;
	std::ostringstream ss;
	uint8_t *data_out;
	size_t data_len;
    if (v.type == Redis::REDIS_RESPONSE_TYPE_ARRAY) {
        if(v.arr_value.values.size() == 3 && v.arr_value.values[2].first == Redis::REDIS_RESPONSE_TYPE_STRING) {
            if(v.arr_value.values[1].second.value._str.compare(MM::sb_mm_channel) == 0) {
				std::vector<std::string> vec = OS::KeyStringToVector(v.arr_value.values[2].second.value._str);
				if (vec.size() < 1) return;
				type = vec.at(0);
				if (!type.compare("send_msg")) {
					if (vec.size() < 7) return;
					gamename = vec.at(1);
					from_ip = vec.at(2);
					from_port = vec.at(3);
					to_ip = vec.at(4);
					to_port = vec.at(5);
					data = vec.at(6);

					ss << to_ip << ":" << to_port;
					
					OS::Base64StrToBin((const char *)data.c_str(), &data_out, data_len);
					if(memcmp(test_message, data_out, test_message_size) == 0) {
						exit(0);
					} else {
						exit(-1);
					}
					free(data_out);
				}
            }
        }
    }
}
void *setup_redis_async(OS::CThread *thread) {
    uv_timespec64_t t;
    t.tv_usec = 0;
    t.tv_sec = 60;
    mp_servermsg_redis_conn = Redis::Connect(OS::g_redisAddress, t);

    Redis::LoopingCommand(mp_servermsg_redis_conn, 60, onRedisMessage, thread->getParams(), "SUBSCRIBE %s", MM::sb_mm_channel);
    return NULL;
}

int main() {
	OS::Init("serverbrowsing", "openspy.cfg");

    mp_async_thread = OS::CreateThread(setup_redis_async, NULL, true);

	test_gameserver = new SBServer();
	test_gameserver->init();

	test_netio_iface = new Test::TestNetIOInterface<>();
	test_netio_iface->fake_SetSendCallback(Fake_SendCallback);

	test_gameserver->setNetIOInterface(test_netio_iface);
	
    SB::Driver *driver = new SB::Driver(test_gameserver, "127.0.0.1", 28910, 2);

	test_gameserver->addNetworkDriver(driver);

	OS::Sleep(1000);

    OS::Address address;
    address.ip = rand();
    address.port = rand();
    Make_FakeConnection(driver, address);

	driver->think(true); //trigger TCP accept
	MapTestConnections(driver);

	std::vector<INetPeer *> peers = driver->getPeers();
	std::map<OS::Address, SBTestState *>::iterator it = m_test_connections.begin();
	while (it != m_test_connections.end()) {
		SBTestState *state = (*it).second;
		Send_FakeServerNoServerReq(state, "gmtest");
		Send_FakeServerMsgReq(state, OS::Address("55.11.22.11:666"), test_message, test_message_size);
		it++;
	}
	
	OS::Sleep(75000);

	it = m_test_connections.begin();
	while (it != m_test_connections.end()) {
		std::pair<OS::Address, SBTestState *> entry = *it;

		Test::TestNetIOInterface<> *net_iface = (Test::TestNetIOInterface<> *)entry.second->driver->getServer()->getNetIOInterface();

		net_iface->fake_TCPDisconnect(entry.second->socket);
		entry.second->peer->DecRef();
		entry.second->peer->think(true);
		it++;
	}

    return -1;
}