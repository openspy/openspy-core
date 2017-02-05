#!/usr/bin/python3

import re
import redis
import MySQLdb

# Open database connection
db = MySQLdb.connect(user='openspy', password='openspy', database='Gamemaster')
redis_ctx = redis.StrictRedis(host='localhost', port=6379, db = 2)

# prepare a cursor object using cursor() method
cursor = db.cursor()

# execute SQL query using execute() method.
cursor.execute("SELECT id, gamename, secretkey,description from games")


for (id, gamename, secretkey, description) in cursor:
	game_key = "{}:{}".format(gamename, id)
	redis_ctx.hset(game_key,"gameid", id)
	redis_ctx.hset(game_key,"gamename", gamename)
	redis_ctx.hset(game_key,"secretkey", secretkey)
	redis_ctx.hset(game_key,"description", description)
	redis_ctx.hset(game_key,"queryport", "6500")
	redis_ctx.hset(game_key,"disabled_services", "0")

cursor.close()

cursor = db.cursor()
cursor.execute("SELECT grp.gameid `gameid`, grp.groupid `groupid`,grp.name `name`,grp.numservers `numservers`,grp.numwaiting `numwaiting`,grp.password `pass`,g.gamename `gamename`, grp.maxwaiting `maxwaiting`, grp.other `other` from grouplist grp INNER JOIN games g on g.id = grp.gameid")


redis_ctx = redis.StrictRedis(host='localhost', port=6379, db = 1)

for (gameid, groupid, name, numservers, numwaiting, password, gamename, maxwaiting, other) in cursor:
	redis_key = "{}:{}:".format(gamename, groupid)
	redis_ctx.hset(redis_key,"gameid", gameid)
	redis_ctx.hset(redis_key,"groupid", groupid)
	redis_ctx.hset(redis_key,"maxwaiting", maxwaiting)
	redis_ctx.hset(redis_key,"hostname", name)
	redis_ctx.hset(redis_key,"password", password)
	redis_ctx.hset(redis_key,"numwaiting", numwaiting)

	other_params = other.split('\\');
	key = None
	custkeys_key = "{}custkeys".format(redis_key)
	for (idx, param) in enumerate(other_params[1:]):
		if not idx % 2:
			key = param
		else:
			redis_ctx.hset(custkeys_key, key, param)

# disconnect from server
db.close()