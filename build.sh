#!/usr/bin/env bash
cd /vagrant
cmake -G "Unix Makefiles" -DCBUILD_TYPE=Debug
make