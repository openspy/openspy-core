#!/usr/bin/env bash
if [ -f /var/openspy/bootstrap.sh ]; then
  cd /var/openspy/ && cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE="Debug" && make
  systemctl daemon-reload
fi
