FROM os-build:latest
ENV DEBIAN_FRONTEND noninteractive
RUN apt-get update && apt-get -y dist-upgrade
RUN apt-get install -y libjemalloc-dev
#COPY fs-out.tar.gz /root/fs.tar.gz
#RUN tar -xvzf /root/fs.tar.gz -C /
#RUN echo /opt/openspy/lib >> /etc/ld.so.conf.d/openspy.conf
#RUN echo /usr/local/lib/x86_64-linux-gnu  >> /etc/ld.so.conf.d/usr_libs.conf
#RUN echo LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libjemalloc.so.1 >> /etc/environment
#RUN /sbin/ldconfig
#WORKDIR /opt/openspy
COPY . /app
COPY docker-support /app
WORKDIR /app
RUN cmake . -DCMAKE_BUILD_TYPE="Debug"
RUN make
RUN echo /usr/local/lib/x86_64-linux-gnu  >> /etc/ld.so.conf.d/usr_libs.conf
RUN /sbin/ldconfig
#ENTRYPOINT ["bash", "/opt/openspy/run.sh"]