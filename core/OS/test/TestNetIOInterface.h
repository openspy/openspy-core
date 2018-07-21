#ifndef _TESTNETIOINTERFACE_H
#define _TESTNETIOINTERFACE_H
#include <OS/OpenSpy.h>
#include <OS/Net/NetIOInterface.h>
#include <vector>
#include <algorithm>
namespace Test {

	template<typename S = INetIOSocket>
	class TestNetIOInterface : public INetIOInterface<S> {
	public:
		TestNetIOInterface() { };

		//socket setup
		INetIOSocket *BindTCP(OS::Address bind_address) {
			return NULL;
		}

		std::vector<INetIOSocket *> TCPAccept(INetIOSocket *socket) {
			std::vector<INetIOSocket *> ret = m_TCPAccept_WaitList;
			m_TCPAccept_WaitList.clear();
			return ret;
		}

		NetIOCommResp streamRecv(INetIOSocket *socket, OS::Buffer &buffer) {
			NetIOCommResp resp;

			std::vector<OS::Buffer> buffer_list = m_pending_packets[socket];
			m_pending_packets[socket].clear();

			std::vector<OS::Buffer>::iterator it = buffer_list.begin();
			while (it != buffer_list.end()) {
				OS::Buffer buf = *it;
				buffer.WriteBuffer(buf.GetHead(), buf.readRemaining());
				it++;
			}

			resp.comm_len = buffer.bytesWritten();
			buffer.resetReadCursor();

			if (resp.comm_len == 0) {
				resp.disconnect_flag = true;
				resp.error_flag = true;
			}
			else {
				resp.disconnect_flag = false;
				resp.error_flag = false;
			}
			

			//std::vector<INetIOSocket *> m_pending_tcp_disconnects;
			std::vector<INetIOSocket *>::iterator dc_it = std::find(m_pending_tcp_disconnects.begin(), m_pending_tcp_disconnects.end(), socket);
			if (dc_it != m_pending_tcp_disconnects.end()) {
				m_pending_tcp_disconnects.erase(dc_it);
				resp.comm_len = 0;
				buffer.resetWriteCursor();
				resp.disconnect_flag = true;
			}
			return resp;
		}

		NetIOCommResp streamSend(INetIOSocket *socket, OS::Buffer &buffer) {
			NetIOCommResp resp;
			resp.comm_len = buffer.bytesWritten();
			resp.packet_count = 1;
			bool error_flag = mp_SendCallback(socket, buffer);
			resp.disconnect_flag = error_flag;
			resp.error_flag = error_flag;
			return resp;
		}

		INetIOSocket *BindUDP(OS::Address bind_address) {
			return NULL;
		}

		NetIOCommResp datagramRecv(INetIOSocket *socket, std::vector<INetIODatagram> &datagrams) {
			NetIOCommResp resp;
			return resp;
		}

		NetIOCommResp datagramSend(INetIOSocket *socket, OS::Buffer &buffer) {
			NetIOCommResp resp;
			return resp;
		}

		void closeSocket(INetIOSocket *socket) {

		}

		void flushSendQueue() {

		}

		void flushSocketFromSendQueue(INetIOSocket *socket) {

		}

		S *createSocket() {
			return new S;
		}


        //fake connection functions
        void fake_AddTCPConnection(INetIOSocket *socket);
		void fake_PushIncomingPacket(INetIOSocket *socket, OS::Buffer &buffer);
		void fake_TCPDisconnect(INetIOSocket *socket);
		void fake_SetSendCallback(bool (*SendCallback)(INetIOSocket *, OS::Buffer));
        protected:
		bool (*mp_SendCallback)(INetIOSocket *, OS::Buffer);
        std::vector<INetIOSocket *> m_TCPAccept_WaitList;
		std::map<INetIOSocket *, std::vector<OS::Buffer> > m_pending_packets;
		std::vector<INetIOSocket *> m_pending_tcp_disconnects;
	};
}
#endif // _TESTNETIOINTERFACE_H