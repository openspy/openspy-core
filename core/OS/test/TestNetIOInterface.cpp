#include "TestNetIOInterface.h"

namespace Test {
	template<>
	void TestNetIOInterface<>::fake_AddTCPConnection(INetIOSocket *socket) {
		m_TCPAccept_WaitList.push_back(socket);
	}
	template<>
    void TestNetIOInterface<>::fake_PushIncomingPacket(INetIOSocket *socket, OS::Buffer &buffer) {
        m_pending_packets[socket].push_back(buffer);
    }
	template<>
	void TestNetIOInterface<>::fake_SetSendCallback(bool (*SendCallback)(INetIOSocket *, OS::Buffer)) {
		mp_SendCallback = SendCallback;
	}
	template<>
	void TestNetIOInterface<>::fake_TCPDisconnect(INetIOSocket *socket) {
		m_pending_tcp_disconnects.push_back(socket);
	}
}