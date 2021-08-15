#include "TCPDriver.h"

TCPDriver::TCPDriver(INetServer *server, const char *host, uint16_t port, bool proxyHeaders, const char *x509_path, const char *rsa_priv_path, SSLNetIOIFace::ESSL_Type ssl_version) : INetDriver(server) {
    OS::Address bind_address(0, port);
    if(x509_path != NULL && rsa_priv_path != NULL  && ssl_version != SSLNetIOIFace::ESSL_None) {
        mp_net_io_interface = (INetIOInterface<> *)new SSLNetIOIFace::SSLNetIOInterface(ssl_version, rsa_priv_path, x509_path);
    } else {
        mp_net_io_interface = new BSDNetIOInterface<>();
    }
    mp_socket = getNetIOInterface()->BindTCP(bind_address);

    gettimeofday(&m_server_start, NULL);

    m_proxy_headers = proxyHeaders;

    mp_mutex = OS::CreateMutex();
    mp_thread = OS::CreateThread(TCPDriver::TaskThread, this, true);
}
TCPDriver::~TCPDriver() {
    mp_thread->SignalExit(true);
    delete mp_thread;
    delete mp_mutex;

    DeleteClients();

    delete mp_socket;
    delete mp_net_io_interface;
}
void TCPDriver::think(bool listener_waiting) {
    if (listener_waiting) {
        std::vector<INetIOSocket *> sockets = getNetIOInterface()->TCPAccept(mp_socket);
        std::vector<INetIOSocket *>::iterator it = sockets.begin();
        while (it != sockets.end()) {
            INetIOSocket *sda = *it;
            if (sda == NULL) return;
            INetPeer *peer = CreatePeer(sda);//new INetPeer(this, sda);

            mp_mutex->lock();

            peer->SetAddress(sda->address);

            m_server->RegisterSocket(peer, m_proxy_headers);
            if(!m_proxy_headers)
                peer->OnConnectionReady();
            
            mp_peers->AddToList(peer);
            
            mp_mutex->unlock();
            it++;
        }
    }
}

INetIOSocket *TCPDriver::getListenerSocket() const {
    return mp_socket;
}
void *TCPDriver::TaskThread(OS::CThread *thread) {
    TCPDriver *driver = (TCPDriver *)thread->getParams();
    while (thread->isRunning()) {
        driver->mp_mutex->lock();

        driver->TickConnections();
        driver->mp_mutex->unlock();
        OS::Sleep(DRIVER_THREAD_TIME);
    }
    return NULL;
}
void TCPDriver::TickConnections() {
    OS::LinkedListIterator<INetPeer*, TCPDriver*> iterator(mp_peers);
    iterator.Iterate(LLIterator_TickOrDeleteClient, this);
}
bool TCPDriver::LLIterator_TickOrDeleteClient(INetPeer* peer, TCPDriver* driver) {
    if (peer->ShouldDelete()) {
        if (peer->GetRefCount() == 1) { //only 1 reference (the drivers reference)
            driver->mp_peers->RemoveFromList(peer);
            driver->m_server->UnregisterSocket(peer);
            peer->DecRef();
            delete peer;
        }
    }
    else {
        peer->think(false);
    }
    return true;
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
bool TCPDriver::LLIterator_DeleteAllClients(INetPeer* peer, TCPDriver* driver) {
    //no need to remove from linked list, since this method is *only* called on TCPDriver delete
    driver->m_server->UnregisterSocket(peer);
    delete peer;
    return true;
}
void TCPDriver::DeleteClients() {
    OS::LinkedListIterator<INetPeer*, TCPDriver*> iterator(mp_peers);
    iterator.Iterate(LLIterator_DeleteAllClients, this);
}