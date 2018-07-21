#include "TestNetIOInterface.h"

namespace Test {
	void TestNetIOInterface<>::fake_AddTCPConnection(INetIOSocket *socket) {
		m_TCPAccept_WaitList.push_back(socket);
	}
    void TestNetIOInterface<>::fake_PushIncomingPacket(INetIOSocket *socket, OS::Buffer &buffer) {
        m_pending_packets[socket].push_back(buffer);
    }
	void TestNetIOInterface<>::fake_SetSendCallback(bool (*SendCallback)(INetIOSocket *, OS::Buffer)) {
		mp_SendCallback = SendCallback;
	}
	void TestNetIOInterface<>::fake_TCPDisconnect(INetIOSocket *socket) {
		m_pending_tcp_disconnects.push_back(socket);
	}
}