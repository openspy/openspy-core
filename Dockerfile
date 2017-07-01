# our base image
FROM ubuntu:latest

ENV REDIS_SERV=openspy.u95v0m.0001.use1.cache.amazonaws.com
ENV REDIS_PORT=6379

EXPOSE 6667

RUN mkdir /openspy
ADD bootstrap.sh /openspy/bootstrap.sh
ADD CMakeLists.txt /openspy/CMakeLists.txt
ADD core /openspy/core
ADD peerchat /openspy/peerchat
ADD gamestats /openspy/gamestats
ADD GP /openspy/GP
ADD natneg /openspy/natneg
ADD qr /openspy/qr
ADD search /openspy/search
ADD serverbrowsing /openspy/serverbrowsing

RUN chmod a+x /openspy/bootstrap.sh
RUN /openspy/bootstrap.sh

RUN cd /openspy && cmake -G "Unix Makefiles" -DCBUILD_TYPE=Debug && make

# Run it
ENTRYPOINT ["gdb", "-ex", "run", "/openspy/bin/peerchat"]
