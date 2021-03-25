# OpenSpy Core - Design
## Main loop
The main loop is shown inside the respective projects main. From there you can see that an instance of INetServer is created, and that there is an infinite loop, where the tick() method is called. This tick essentially calls the registered drivers tick method (currently named think), for each registered driver. This essentially performs the TCP Accept call (assuming a TCP socket), or in the case of UDP sockets (such as NatNeg, currently QR introduces stateful UDP connections) a recv call occurs, and any packets are processed. Once all driver tick methods have been called, the NetworkTick method is called, which performs the multiplexer logic to perform packet processing. This is done for both TCP and UDP services. The NetworkTick method is where the main thread idles, waiting for incoming data to process. After a period of time it times out, and will allow the main thread to tick once, to handle things like ping logic.

It is important that the main thread is left idle as much as possible. All IO operations should be performed using an async task.

## Multiplexers
* EPoll
* Select (win32 dev/fallback)

## Server/Driver/Peer model

### Server
This is the encalsulation of the entire process, containing many drivers. The server is used to perform stuff that is global to the service as a whole, such as looking up a user by IP, who may be using either version 1 or 2 of the server browsing service.

### Driver
The driver is the instance of the listening service. At this point in time it is only used to listen on 1 port at a time, and probably should remain this way. Depending on the function of the service, you may be able to get away with only having 1 implementation of this for multiple versions. In the case of server browsing, 1 driver exists, and the peer handles the actual packet parsing. Due to the similarities from version 1 and 2, the same driver can be used (albeit a seperate instance of it for each version), with a seperate peer class being used depending on the version.

In the event of stateless services, the driver handles packet processing and associated logic.
### Peer (TCP only)
This should only be created in a TCP service. Currently QR, being a UDP service, does create a stateful Peer, but this goes against the very principal of UDP being a stateless connection, and has caused bugs that can be overcome by removing this implementation. Most UDP services on GameSpy do have a concept of state, but that can be handled using the async IO system, and a peer instance is not truly needed.

This class is essentially the connection state, and handles parsing, ping checks, events and so on.

The parent class implemented OS::Ref for reference conuting. It is important to maintain the reference count properly so that connections can be cleaned up when no longer in use.

## Async task system
The async task system handles all IO dependant tasks, such as HTTP calls, redis calls, and anything which updates, fetches data, or any form of blocking call. By using a task pool, multiple tasks can be performed as needed. The numer of instances in the task pool is configurable at run time, by environment variable or otherwise, using the openspy.xml file

It is important to increment and decrement the reference of INetPeer if any such instance is passed. Failure to do so can result in the application crashing, if the user disconnects, or the connection staying open forever.