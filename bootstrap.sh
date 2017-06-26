#!/usr/bin/env bash
export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get dist-upgrade -y
apt-get install -y build-essential cmake libhiredis-dev libevent-dev screen gdb libssl-dev libcurl4-openssl-dev libssl-dev libjansson-dev


apt-get install -y software-properties-common python-software-properties
add-apt-repository -y ppa:ben-collins/libjwt
apt-get update
apt-get install -y libjwt-dev


if [ ! -f /usr/include/jwt/jwt.h ]; then
    mkdir /usr/include/jwt
    ln -s /usr/include/jwt.h /usr/include/jwt/jwt.h
fi

service_names="serverbrowsing
qr
peerchat"
if [ -f /var/openspy/bootstrap.sh ]; then
    for name in $service_names
    do
        sed 's/OS_PROJ_NAME/$name/g' /var/openspy/os_service/template > /etc/init.d/$name
        sed 's/OS_PROJ_NAME/$name/g' /var/openspy/os_service/template.conf > /etc/init/$name.conf
        sed 's/OS_PROJ_NAME/$name/g' /var/openspy/os_service/template.service > /lib/systemd/system/$name.service
        chmod +x /etc/init.d/$name
        chmod +x /etc/init/$name.conf
        chmod +x /lib/systemd/system/$name.service
    done
fi


