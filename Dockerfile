FROM ubuntu:24.04 AS build
ENV DEBIAN_FRONTEND noninteractive
RUN apt-get update
RUN apt-get install -y build-essential cmake git curl unzip zip pkg-config

RUN git clone https://github.com/microsoft/vcpkg.git
ENV VCPKG_ROOT /vcpkg
ENV PATH "$PATH:$VCPKG_ROOT"
WORKDIR /vcpkg
RUN ./bootstrap-vcpkg.sh
ENV VCPKG_TC_PATH "$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"


RUN mkdir /root/fs-out

WORKDIR /root/

#install deps
COPY vcpkg.json /root/
COPY vcpkg-configuration.json /root/
RUN vcpkg install

#we need to dynamically generate a script due to weirdness of vcpkg + cmake within docker (essentially the cmake packages won't find it otherwise)
RUN echo "export LIBUV_DIR=`realpath /vcpkg/packages/libuv*/`" >> build.sh
RUN echo "export OPENSSL_ROOT_DIR=`realpath /vcpkg/packages/openssl*/`" >> build.sh
RUN echo "export HIREDIS_ROOT_DIR=`realpath /vcpkg/packages/hiredis*/`" >> build.sh
RUN echo "export JANSSON_ROOT_DIR=`realpath /vcpkg/packages/jansson*/`" >> build.sh
RUN echo "export PUGIXML_ROOT_DIR=`realpath /vcpkg/packages/pugixml*/share/pugixml`" >> build.sh
RUN echo "export RABBITMQC_ROOT_DIR=`realpath /vcpkg/packages/librabbitmq*/share/rabbitmq-c`" >> build.sh
RUN echo "export CURL_INCLUDE_DIR=`realpath /vcpkg/packages/curl*/include/`" >> build.sh
RUN echo "export CURL_LIBRARY=`realpath /vcpkg/packages/curl*/lib/libcurl.a`" >> build.sh
RUN echo "export ZLIB_LIBRARY=`realpath /vcpkg/packages/zlib*/lib/libz.a`" >> build.sh
RUN echo "export ZLIB_INCLUDE_DIR=`realpath /vcpkg/packages/zlib*/include`" >> build.sh



RUN echo 'cmake -DZLIB_INCLUDE_DIR=$ZLIB_INCLUDE_DIR -DZLIB_LIBRARY=$ZLIB_LIBRARY -DCURL_INCLUDE_DIR=$CURL_INCLUDE_DIR -DCURL_LIBRARY=$CURL_LIBRARY -DLIBUV_DIR=$LIBUV_DIR -DHIREDIS_ROOT_DIR=$HIREDIS_ROOT_DIR -DHIREDIS_SSL_ROOT_DIR=$HIREDIS_ROOT_DIR -DJANSSON_ROOT_DIR=$JANSSON_ROOT_DIR -Dpugixml_DIR=$PUGIXML_ROOT_DIR -Drabbitmq-c_DIR=$RABBITMQC_ROOT_DIR -DCMAKE_TOOLCHAIN_FILE=$VCPKG_TC_PATH -DCMAKE_BINARY_DIR="/root/os-bin" -DBUILD_SHARED_LIBS:BOOL=ON -DCMAKE_BUILD_TYPE="Release" -DVCPKG_BUILD_TYPE=release ../code' >> build.sh
RUN chmod +x build.sh

COPY cmake /root/cmake
COPY code /root/code
RUN mkdir /root/os-bin /root/os-make
WORKDIR /root/os-make
run ../build.sh
run make
RUN mkdir -p /root/fs-out/opt/openspy/bin /root/fs-out/opt/openspy/lib
RUN mv /root/os-make/bin/* /root/fs-out/opt/openspy/bin
RUN mv /root/os-make/lib/* /root/fs-out/opt/openspy/lib
COPY docker-support/run.sh /root/fs-out/opt/openspy/run.sh
RUN chmod a+x /root/fs-out/opt/openspy/run.sh

WORKDIR /root/fs-out
RUN tar -cvzf /fs-out.tar.gz *

FROM ubuntu:24.04
ENV DEBIAN_FRONTEND noninteractive
RUN apt-get update && apt-get -y dist-upgrade
COPY --from=build /fs-out.tar.gz /root/fs.tar.gz
RUN tar -xvzf /root/fs.tar.gz -C /
RUN echo /opt/openspy/lib >> /etc/ld.so.conf.d/openspy.conf
RUN echo /usr/local/lib/x86_64-linux-gnu  >> /etc/ld.so.conf.d/usr_libs.conf
RUN /sbin/ldconfig
WORKDIR /opt/openspy
ENTRYPOINT ["/bin/bash", "/opt/openspy/run.sh"]