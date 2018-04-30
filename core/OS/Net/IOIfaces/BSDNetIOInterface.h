#ifndef _BSDNETIOINTERFACE_H
#define _BSDNETIOINTERFACE_H
#include <OS/OpenSpy.h>
#include <OS/Net/NetIOInterface.h>
#include <vector>
#include <algorithm>

template<typename S = INetIOSocket>
class BSDNetIOInterface : public INetIOInterface<S> {
	public:
		BSDNetIOInterface() {

		}
		~BSDNetIOInterface() {

		}
		//NET IO INTERFACE
		INetIOSocket *BindTCP(OS::Address bind_address) {
			INetIOSocket *net_socket = this->createSocket();
			net_socket->address = bind_address;

			if ((net_socket->sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
				//signal error
				perror("socket()");
				goto end_error;
			}
			int on = 1;
			if (setsockopt(net_socket->sd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on))
				< 0) {
				perror("setsockopt()");
				//signal error
				goto end_error;
			}
#if SO_REUSEPORT
			if (setsockopt(net_socket->sd, SOL_SOCKET, SO_REUSEPORT, (char *)&on, sizeof(on))
				< 0) {
				perror("setsockopt()");
				//signal error
				goto end_error;
			}
#endif
			struct sockaddr_in addr;

			addr = bind_address.GetInAddr();
			addr.sin_family = AF_INET;
			int n = bind(net_socket->sd, (struct sockaddr *)&addr, sizeof addr);
			if (n < 0) {
				perror("bind()");
				//signal error
				goto end_error;
			}
			if (listen(net_socket->sd, SOMAXCONN)
				< 0) {
				perror("listen()");
				//signal error
				goto end_error;
			}
			makeNonBlocking(net_socket->sd);
			return net_socket;
		end_error:
			if (net_socket)
				delete net_socket;

			return NULL;
		}
		std::vector<INetIOSocket *> TCPAccept(INetIOSocket *socket) {
			std::vector<INetIOSocket *> ret;
			INetIOSocket *incoming_socket;
			while (true) {
				socklen_t psz = sizeof(struct sockaddr_in);
				struct sockaddr_in peer;
				int sda = accept(socket->sd, (struct sockaddr *)&peer, &psz);
				if (sda <= 0) break;
				makeNonBlocking(sda);
				incoming_socket = this->createSocket();
				incoming_socket->sd = sda;
				incoming_socket->address = peer;
				ret.push_back(incoming_socket);
			}
			return ret;
		}
		NetIOCommResp streamRecv(INetIOSocket *socket, OS::Buffer &buffer) {
			NetIOCommResp ret;

			char recvbuf[1492];
			buffer.reset();
			while (true) {
				int len = recv(socket->sd, recvbuf, sizeof recvbuf, 0);
				if (len <= 0) {
					if (len == 0) {
						ret.error_flag = true;
						ret.disconnect_flag = true;
					}
					goto end;
				}
				ret.comm_len += len;
				ret.packet_count++;
				buffer.WriteBuffer(recvbuf, len);
			}// while (errno != EWOULDBLOCK && errno != EAGAIN); //only check when data is available
		end:
			return ret;
		}
		NetIOCommResp streamSend(INetIOSocket *socket, OS::Buffer &buffer) {
			NetIOCommResp ret;

			ret.comm_len = send(socket->sd, (const char *)buffer.GetHead(), buffer.size(), MSG_NOSIGNAL);
			if (ret.comm_len < 0) {
				if (ret.comm_len == -1) {
					ret.disconnect_flag = true;
					ret.error_flag = true;
				}
			}
			if (ret.comm_len != buffer.size()) {
				switch (errno) {
				case EAGAIN:
				#if EWOULDBLOCK != EAGAIN
				case EWOULDBLOCK:
				#endif
					//queue data for resend
					m_stream_send_queue[socket].push_back(buffer);
					break;
				}
			}
			return ret;
		}

		INetIOSocket *BindUDP(OS::Address bind_address) {
			INetIOSocket *net_socket = this->createSocket();
			net_socket->address = bind_address;

			if ((net_socket->sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
				//signal error
				perror("socket()");
				goto end_error;
			}
			int on = 1;
			if (setsockopt(net_socket->sd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on))
				< 0) {
				//signal error
				perror("setsockopt()");
				goto end_error;
			}
#if SO_REUSEPORT
			if (setsockopt(net_socket->sd, SOL_SOCKET, SO_REUSEPORT, (char *)&on, sizeof(on))
				< 0) {
				//signal error
				perror("setsockopt()");
				goto end_error;
			}
#endif
			struct sockaddr_in addr;

			addr = bind_address.GetInAddr();
			addr.sin_family = AF_INET;
			int n = bind(net_socket->sd, (struct sockaddr *)&addr, sizeof addr);
			if (n < 0) {
				perror("bind()");
				//signal error
				goto end_error;
			}
			makeNonBlocking(net_socket->sd);
			return net_socket;
		end_error:
			if (net_socket)
				free((void *)net_socket);

			return NULL;
		}
		NetIOCommResp datagramRecv(INetIOSocket *socket, std::vector<INetIODatagram> &datagrams) {
			NetIOCommResp ret;
			std::vector<INetIODatagram>::iterator it;
			char recvbuf[1492];
			OS::Address os_addr;
			sockaddr_in in_addr;
			while (true) {
				INetIODatagram dgram;
				socklen_t in_len = sizeof(in_addr);
				int len = recvfrom(socket->sd, (char *)&recvbuf, sizeof(recvbuf), 0, (struct sockaddr *)&in_addr, &in_len);
				if (len <= 0) {
					break;
				}
				os_addr = in_addr;

				it = std::find(datagrams.begin(), datagrams.end(), os_addr);
				if (it != datagrams.end()) {
					dgram = *it;
				}

				dgram.address = in_addr;
				dgram.buffer = OS::Buffer(len);
				dgram.buffer.WriteBuffer(recvbuf, len);
				dgram.buffer.reset();

				if (it != datagrams.end()) {
					*it = dgram;
				}
				else {
					datagrams.push_back(dgram);
				}
			} //while (errno != EWOULDBLOCK && errno != EAGAIN);
			return ret;
		}
		NetIOCommResp datagramSend(INetIOSocket *socket, OS::Buffer &buffer) {
			NetIOCommResp ret;
			ret.comm_len = sendto(socket->sd, (const char *)buffer.GetHead(), buffer.size(), MSG_NOSIGNAL, (const sockaddr *)&socket->address.GetInAddr(), sizeof(sockaddr));
			if (ret.comm_len != buffer.size()) {
				switch (errno) {
				case EAGAIN:
				#if EWOULDBLOCK != EAGAIN
				case EWOULDBLOCK:
				#endif
					//queue data for resend
					m_datagram_send_queue[socket].push_back(buffer);
					break;
				}
			}
			return ret;
		}
		void closeSocket(INetIOSocket *socket) {
			if (!socket->shared_socket)
				close(socket->sd);
			flushSocketFromSendQueue(socket);
			delete socket;
		}

		void flushSendQueue() {
			std::vector<OS::Buffer>::iterator it2;
			std::map<INetIOSocket *, std::vector<OS::Buffer> > map_cpy = m_datagram_send_queue;
			m_datagram_send_queue.clear();

			//send UDP pending packets
			std::map<INetIOSocket *, std::vector<OS::Buffer> >::iterator it = map_cpy.begin();
			while (it != map_cpy.end()) {
				std::vector<OS::Buffer> send_list = (*it).second;
				it2 = send_list.begin();
				while (it2 != send_list.end()) {
					this->datagramSend((*it).first, *it2);
					it2++;
				}
				it++;
			}

			//send TCP pending packets
			map_cpy = m_stream_send_queue;
			it = map_cpy.begin();
			while (it != map_cpy.end()) {
				std::vector<OS::Buffer> send_list = (*it).second;
				it2 = send_list.begin();
				while (it2 != send_list.end()) {
					this->streamSend((*it).first, *it2);
					it2++;
				}
				it++;
			}
		}

		void flushSocketFromSendQueue(INetIOSocket *socket) {
			std::map<INetIOSocket *, std::vector<OS::Buffer> >::iterator it = m_datagram_send_queue.begin();
			while (it != m_datagram_send_queue.end()) {
				if ((*it).first == socket) {
					m_datagram_send_queue.erase(it);
					it = m_datagram_send_queue.begin();
					continue;
				}
				it++;
			}

			it = m_stream_send_queue.begin();
			while (it != m_stream_send_queue.end()) {
				if ((*it).first == socket) {
					m_stream_send_queue.erase(it);
					it = m_stream_send_queue.begin();
					continue;
				}
				it++;
			}
		}
		void makeNonBlocking(int sd) {
			unsigned long mode = 1;
			#ifdef _WIN32
				ioctlsocket(sd, FIONBIO, &mode);
			#elif defined(O_NONBLOCK)
				/* Fixme: O_NONBLOCK is defined but broken on SunOS 4.1.x and AIX 3.2.5. */
				if (-1 == (mode = fcntl(sd, F_GETFL, 0)))
					mode = 0;
				mode |= O_NONBLOCK;
				fcntl(sd, F_SETFL, mode);
			#else
				/* Otherwise, use the old way of doing it */
				ioctl(sd, FIONBIO, &mode);
			#endif
		}

		S *createSocket() {
			return new S;
		}
		//
	protected:
		std::map<INetIOSocket *, std::vector<OS::Buffer> > m_datagram_send_queue;
		std::map<INetIOSocket *, std::vector<OS::Buffer> > m_stream_send_queue;
};

#endif //_BSDNETIOINTERFACE_H