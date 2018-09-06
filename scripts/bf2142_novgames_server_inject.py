#!/usr/bin/python3

import re
import redis

def create_server(gamename, basic_info, cust_keys, player_keys):
    server_id = redis_ctx.incr("QRID")
    server_key = "{}:0:{}:".format(gamename, server_id)

    basic_info["id"] = "{}".format(server_id)

    for key, value in basic_info.items():
        redis_ctx.hset(server_key, key, value)

    for key, value in cust_keys.items():
        redis_ctx.hset("{}custkeys".format(server_key), key, value)

    redis_ctx.zincrby(gamename, server_key)


# Open database connection
redis_ctx = redis.StrictRedis(host='10.1.1.125', port=6379, db = 0)

novgames_servers = [
    { "ip":"62.118.131.204", "port":"29980"},
    { "ip":"62.118.131.204", "port":"29987"},
    { "ip":"62.118.131.204", "port":"29990"},
    { "ip":"62.118.131.204", "port":"20502"},
    { "ip":"62.118.131.204", "port":"30087"},
    { "ip":"62.118.131.204", "port":"30089"},
    { "ip":"62.118.131.204", "port":"30090"},
    { "ip":"185.66.87.101", "port":"29997"},
    { "ip":"162.248.91.125", "port":"29997"},
    { "ip":"162.248.91.125", "port":"29998"},
    { "ip":"185.92.221.92", "port":"29900"},
    { "ip":"45.32.202.165", "port":"29900"},
    { "ip":"82.151.200.106", "port":"29301"},
    { "ip":"82.151.200.106", "port":"29300"}
]

for entry in novgames_servers:
    basic_info = {}
    basic_info["gameid"] = "1324"
    basic_info["wan_port"] = entry["port"]
    basic_info["wan_ip"] = entry["ip"]

    cust_keys = {"hostname": "novgames injected server", "localip0": entry["ip"], "localport": entry["port"], "hostport": "17568"}

    create_server("stella", basic_info, cust_keys, {})

