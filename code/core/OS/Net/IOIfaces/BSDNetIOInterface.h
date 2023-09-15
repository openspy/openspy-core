#ifndef _BSDNETIOINTERFACE_H
#define _BSDNETIOINTERFACE_H
#include <OS/OpenSpy.h>
#include <OS/Net/NetIOInterface.h>
#include <vector>
#include <algorithm>

#include <OS/KVReader.h>

template<typename S = INetIOSocket>
class BSDNetIOInterface : public INetIOInterface<S> {
	public:
		BSDNetIOInterface() {

		}
		~BSDNetIOInterface() {

		}
		//NET IO INTERFACE
		INetIOSocket *BindTCP(OS::Address bind_address) {
			struct sockaddr_in addr;
			int n;
			int on = 1;
			INetIOSocket *net_socket = this->createSocket();
			net_socket->address = bind_address;

			if ((net_socket->sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
				//signal error
				perror("socket()");
				goto end_error;
			}
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
			

			addr = bind_address.GetInAddr();
			addr.sin_family = AF_INET;
			n = bind(net_socket->sd, (struct sockaddr *)&addr, sizeof addr);
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
				socktype_t sda = accept(socket->sd, (struct sockaddr *)&peer, &psz);
				#ifdef _WIN32
				if (sda == INVALID_SOCKET) break;
				#else
				if (sda <= 0) break;
				#endif
				
				makeNonBlocking(sda);
				incoming_socket = this->createSocket();
				incoming_socket->sd = sda;
				incoming_socket->address = peer;
				ret.push_back(incoming_socket);
			}
			return ret;
		}
		bool ReadProxyAddress(INetIOSocket *socket, OS::Address &source_address, OS::Address &proxy_server_address) {
			bool read_proxy_address = false;
			char recvbuf[108];
			char *p = (char *)recvbuf;
			while (true) {
				int len = recv(socket->sd, p, 1, 0);
				if (len <= 0 || *p == '\n') {
					*(++p) = 0;
					break;
				}
				read_proxy_address = true;
				p++;
			}
			if(read_proxy_address) {
				OS::KVReader kv_reader(recvbuf, ' ');
				std::pair<std::string, std::string> ip_info = kv_reader.GetPairByIdx(1);
				std::pair<std::string, std::string> port_info = kv_reader.GetPairByIdx(2);

				std::string ip_string = ip_info.first + ":" + port_info.first;
				source_address = OS::Address(ip_string);

				ip_string = ip_info.second + ":" + port_info.second;
				proxy_server_address = OS::Address(ip_string);
			}
			return read_proxy_address;
		}
		NetIOCommResp streamRecv(INetIOSocket *socket, OS::Buffer &buffer) {
			NetIOCommResp ret;

			char recvbuf[1492];
			buffer.resetWriteCursor();
			while (true) {
				int len = recv(socket->sd, recvbuf, sizeof recvbuf, 0);
				if (len <= 0) {
					if (len == 0) {
						ret.disconnect_flag = true;
					}
					else {
						bool wouldBlock = false;
						#ifdef _WIN32
						int wsaerr = WSAGetLastError();
						wouldBlock = wsaerr == WSAEWOULDBLOCK;
						#else
						int err = errno;
						wouldBlock = err == EWOULDBLOCK;
						#endif
						if (!wouldBlock) {
							ret.disconnect_flag = true;
							ret.error_flag = true;
						}
					}
					goto end;
				}
				ret.comm_len += len;
				ret.packet_count++;
				buffer.WriteBuffer(recvbuf, len);
			}// while (errno != EWOULDBLOCK && errno != EAGAIN); //only check when data is available
		end:
			buffer.resetReadCursor();
			return ret;
		}
		NetIOCommResp streamSend(INetIOSocket *socket, OS::Buffer &buffer) {
			NetIOCommResp ret;

			ret.comm_len = send(socket->sd, (const char *)buffer.GetHead(), (int)buffer.bytesWritten(), MSG_NOSIGNAL);
			if (ret.comm_len < 0) {
				if (ret.comm_len == -1) {
					ret.disconnect_flag = true;
					ret.error_flag = true;
				}
			}
			return ret;
		}

		INetIOSocket *BindUDP(OS::Address bind_address) {
			struct sockaddr_in addr;
			int on = 1;
			int n;			
			INetIOSocket *net_socket = this->createSocket();
			net_socket->address = bind_address;

			if ((net_socket->sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
				//signal error
				perror("socket()");
				goto end_error;
			}

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
			addr = bind_address.GetInAddr();
			addr.sin_family = AF_INET;
			n = bind(net_socket->sd, (struct sockaddr *)&addr, sizeof addr);
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

				dgram.address = in_addr;
				//dgram.buffer = OS::Buffer(len);
				dgram.buffer.WriteBuffer(recvbuf, len);

				dgram.buffer.resetReadCursor();

				dgram.comm_len = len;

				datagrams.push_back(dgram);
			} //while (errno != EWOULDBLOCK && errno != EAGAIN);
			return ret;
		}
		NetIOCommResp datagramSend(INetIOSocket *socket, OS::Buffer &buffer) {
			NetIOCommResp ret;
			const sockaddr_in addr = (const sockaddr_in)socket->address.GetInAddr();
			ret.comm_len = sendto(socket->sd, (const char *)buffer.GetHead(), (int)buffer.bytesWritten(), MSG_NOSIGNAL, (const sockaddr *)&addr, sizeof(sockaddr));
			return ret;
		}
		void closeSocket(INetIOSocket *socket) {
			if (!socket->shared_socket)
				close(socket->sd);
			delete socket;
		}

		void makeNonBlocking(socktype_t sd) {
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
};

#endif //_BSDNETIOINTERFACE_H