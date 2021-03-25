# OpenSpy Core (v2)

This is a full rewrite of the old openspy. Each service runs as a seperate process, communicating over HTTP, AMQP, and also using redis as a data store.

## TODO / Things to improve
* Strip STL out (or at least resolve slow leaking/fragmentation issue)
* Natneg v1 (currently only v1 and v2 are supported)
* Better FESL support for more EA games
* Peerchat


## Building
A docker file has been created to convey the required build and runtime environment. This has not been used for development, but can be helpful for testing and just getting the project running.

## Running
If you refer to the "openspy-web-backend" project, this will have everything you need to get openspy running.
From the perspective of this application, the requirements are redis, rabbitmq, and then the openspy-web-backend.

Depending on the services you will be running, you can get away without running openspy-web-backend, but it is recommended. Redis and rabbitmq are required.