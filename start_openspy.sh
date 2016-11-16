#!/usr/bin/env bash
redis-cli < ./redis_init_dbg.txt
screen -dms sb "gdb -ex r /vagrant/bin/serverbrowsing"
screen -s qr -d -m "gdb -ex r /vagrant/bin/qr"
