#!/usr/bin/python3

import re
import redis
import MySQLdb

# Open database connection
db = MySQLdb.connect(user='openspy', passwd='openspy', db='Gamemaster')
redis_ctx = redis.StrictRedis(port=6379, db = 2, host='localhost')

# prepare a cursor object using cursor() method
cursor = db.cursor()

# execute SQL query using execute() method.
cursor.execute("SELECT id, gamename, secretkey,description,backendflags,queryport,disabledservices from games")


for (id, gamename, secretkey, description, backendflags, queryport, disabledservices) in cursor:
	game_key = "{}:{}".format(gamename, id)
	redis_ctx.hset(game_key,"gameid", id)
	redis_ctx.hset(game_key,"gamename", gamename)
	redis_ctx.hset(game_key,"secretkey", secretkey)
	redis_ctx.hset(game_key,"description", description)
	redis_ctx.hset(game_key,"queryport", queryport)
	redis_ctx.hset(game_key,"backendflags", backendflags)
	redis_ctx.hset(game_key,"disabled_services", disabledservices)

	redis_ctx.set(gamename, game_key)
	redis_ctx.set("gameid_{}".format(id), game_key)

cursor.close()

cursor = db.cursor()
cursor.execute("SELECT grp.gameid `gameid`, grp.groupid `groupid`,grp.name `name`,g.gamename `gamename`, grp.maxwaiting `maxwaiting`, grp.other `other` from grouplist grp INNER JOIN games g on g.id = grp.gameid")


redis_ctx = redis.StrictRedis(host='localhost', port=6379, db = 1)

for (gameid, groupid, name, gamename, maxwaiting, other) in cursor:
	redis_key = "{}:{}:".format(gamename, groupid)
	redis_ctx.hset(redis_key,"gameid", gameid)
	redis_ctx.hset(redis_key,"groupid", groupid)
	redis_ctx.hset(redis_key,"maxwaiting", maxwaiting)
	redis_ctx.hset(redis_key,"hostname", name)

	other_params = other.split('\\')
	key = None
	custkeys_key = "{}custkeys".format(redis_key)
	for (idx, param) in enumerate(other_params[1:]):
		if not idx % 2:
			key = param
		else:
			redis_ctx.hset(custkeys_key, key, param)

# disconnect from server
db.close()
