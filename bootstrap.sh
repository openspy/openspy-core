#!/usr/bin/env bash
export DEBIAN_FRONTEND=noninteractive
#setup server
apt-get update
apt-get dist-upgrade -y
apt-get install -y redis-server mysql-server libmysqlclient-dev build-essential cmake libhiredis-dev libevent-dev screen gdb