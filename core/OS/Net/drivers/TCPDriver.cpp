#include <algorithm>
#include "TCPDriver.h"

TCPDriver::TCPDriver(INetServer *server, const char *host, uint16_t port, bool proxyHeaders, const char *x509_path = NULL, const char *rsa_priv_path = NULL, SSLNetIOIFace::ESSL_Type ssl_version = SSLNetIOIFace::ESSL_None) : INetDriver(server) {
    OS::Address bind_address(0, port);
    if(x509_path != NULL && rsa_priv_path != NULL  && ssl_version != SSLNetIOIFace::ESSL_None) {
        mp_net_io_interface = (INetIOInterface<> *)new SSLNetIOIFace::SSLNetIOInterface(ssl_version, rsa_priv_path, x509_path);
    } else {
        mp_net_io_interface = new BSDNetIOInterface<>();
    }
    mp_socket = getNetIOInterface()->BindTCP(bind_address);

    m_connections.clear();

    gettimeofday(&m_server_start, NULL);
    gettimeofday(&m_last_connection_resize, NULL);

    mp_mutex = OS::CreateMutex();
    mp_thread = OS::CreateThread(TCPDriver::TaskThread, this, true);

    m_proxy_headers = proxyHeaders;
}
TCPDriver::~TCPDriver() {
    mp_thread->SignalExit(true);
    delete mp_mutex;
    delete mp_thread;

    DeleteClients();
}
void TCPDriver::think(bool listener_waiting) {
    if (listener_waiting) {
        std::vector<INetIOSocket *> sockets = getNetIOInterface()->TCPAccept(mp_socket);
        std::vector<INetIOSocket *>::iterator it = sockets.begin();
        while (it != sockets.end()) {
            INetIOSocket *sda = *it;
            if (sda == NULL) return;
            INetPeer *peer = CreatePeer(sda);

            mp_mutex->lock();

            peer->SetAddress(sda->address);

            m_server->RegisterSocket(peer, m_proxy_headers);
            if(!m_proxy_headers)
                peer->OnConnectionReady();
            
            m_connections.push_back(peer);
            
            mp_mutex->unlock();
            it++;
        }
    }
}

const std::vector<INetPeer *> TCPDriver::getPeers(bool inc_ref = false) {
    mp_mutex->lock();
    std::vector<INetPeer *> peers;
    std::deque<INetPeer *>::iterator it = m_connections.begin();
    while (it != m_connections.end()) {
        INetPeer * p = (INetPeer *)*it;
        if(p == NULL) break;
        peers.push_back(p);
        if (inc_ref)
            p->IncRef();
        it++;
    }
    mp_mutex->unlock();
    return peers;
}
INetIOSocket *TCPDriver::getListenerSocket() const {
    return mp_socket;
}
const std::vector<INetIOSocket *> TCPDriver::getSockets() const {
    std::vector<INetIOSocket *> sockets;
    mp_mutex->lock();
    std::deque<INetPeer *>::const_iterator it = m_connections.begin();
    while (it != m_connections.end()) {
        INetPeer *p = *it;
        if(p == NULL) break;
        sockets.push_back(p->GetSocket());
        it++;
    }
    mp_mutex->unlock();
    return sockets;
}

void *TCPDriver::TaskThread(OS::CThread *thread) {
    TCPDriver *driver = (TCPDriver *)thread->getParams();
    while (thread->isRunning()) {
        driver->mp_mutex->lock();
        std::deque<INetPeer *>::iterator it = driver->m_connections.begin();
        while (it != driver->m_connections.end()) {
            INetPeer *peer = *it;
            if(peer == NULL) break;
            if (peer->ShouldDelete() && peer->GetRefCount() >= 1) {
                //marked for delection, dec reference and delete when zero
                peer->DecRef();
                driver->m_server->UnregisterSocket(peer);
            }
            it++;
        }
        driver->CleanClientQueue();
        driver->TickConnections();
        driver->mp_mutex->unlock();
        OS::Sleep(DRIVER_THREAD_TIME);
    }
    return NULL;
}
void TCPDriver::CleanClientQueue() {
    if(m_connections.empty()) return;
    INetPeer *peer = NULL;
    std::deque<INetPeer *> output_queue;
    while (!m_connections.empty()) {
        peer = m_connections.front();
        m_connections.pop_front();

        if(peer->ShouldDelete() && peer->GetRefCount() == 0) {
            delete peer;
        } else {
            if(peer->ShouldDelete()) {
                output_queue.push_front(peer);
            } else {
                output_queue.push_back(peer);
            }
        }
    }

    m_connections.swap(output_queue);

    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    if(current_time.tv_sec - m_last_connection_resize.tv_sec > DRIVER_QUEUE_RESIZE_TIME) {
        m_connections.shrink_to_fit();
        gettimeofday(&m_last_connection_resize, NULL);
    }
}
void TCPDriver::TickConnections() {
    std::deque<INetPeer *>::iterator it = m_connections.begin();
    while (it != m_connections.end()) {
        INetPeer *p = *it;
        if(p == NULL) break;
        if(!p->ShouldDelete())
            p->think(false);
        it++;
    }
}
void TCPDriver::OnPeerMessage(INetPeer *peer) {
    OS::Address source_address, proxy_server_address;
    bool success = getNetIOInterface()->ReadProxyAddress(peer->GetSocket(), source_address, proxy_server_address);

    if(success) {
        m_server->UnregisterSocket(peer);
        m_server->RegisterSocket(peer);
        peer->SetAddress(source_address);
        peer->OnConnectionReady();
    }
}
void TCPDriver::DeleteClients() {
    while(!m_connections.empty()) {
        INetPeer *peer = m_connections.back();
        m_server->UnregisterSocket(peer);
        delete peer;
        m_connections.pop_back();
    }
}
