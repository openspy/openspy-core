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

    key_id = 0
    for player_key_dict in player_keys:
        player_key_name = "{}custkeys_player_{}".format(server_key, key_id)
        key_id = key_id + 1

        for key, value in player_key_dict.items():
            redis_ctx.hset(player_key_name, key, value)



# Open database connection
redis_ctx = redis.StrictRedis(host='10.1.1.125', port=6379, db = 0)

novgames_servers = [
    { "ip":"62.118.131.204", "port":"29980"},

]

for entry in novgames_servers:
    print("IP Combo: {}\n".format(entry))
    basic_info = {}
    basic_info["gameid"] = "1420"
    basic_info["wan_port"] = entry["port"]
    basic_info["wan_ip"] = entry["ip"]

    cust_keys = {"hostname": "flatout2pc test server", "localip0": entry["ip"], "localport": entry["port"], "hostport": "17568", "datachecksum": "3546d58093237eb33b2a96bb813370d846ffcec8", "car_class": "0", "car_type": "5"}

    create_server("flatout2pc", basic_info, cust_keys, [
        {"player_": "a name", "team_": "some team"}
    ])


