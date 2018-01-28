from cgi import parse_qs, escape

import base64
import json
import redis
import os

from Model.Game import Game
from Model.GameGroup import GameGroup

from playhouse.shortcuts import model_to_dict, dict_to_model

from BaseService import BaseService



class GameService(BaseService):
    def __init__(self):
        BaseService.__init__(self)
        self.REDIS_GAMESERVERS_DB = 0
        self.REDIS_GAME_DB = 2
        self.REDIS_GAMEGROUP_DB = 1
        self.redis_ctx = redis.StrictRedis(host=os.environ['REDIS_SERV'], port=int(os.environ['REDIS_PORT']), db = self.REDIS_GAMESERVERS_DB)
        self.redis_game_ctx = redis.StrictRedis(host=os.environ['REDIS_SERV'], port=int(os.environ['REDIS_PORT']), db = self.REDIS_GAME_DB)
        self.redis_group_ctx = redis.StrictRedis(host=os.environ['REDIS_SERV'], port=int(os.environ['REDIS_PORT']), db = self.REDIS_GAMEGROUP_DB)

    def sync_game_to_redis(self, new_data, old_data):
        self.redis_game_ctx.delete("{}:{}".format(old_data["gamename"],old_data["id"]))

        self.redis_game_ctx.hset("{}:{}".format(new_data["gamename"],new_data["id"]), "gameid", new_data["id"])
        self.redis_game_ctx.hset("{}:{}".format(new_data["gamename"],new_data["id"]), "gamename", new_data["gamename"])
        self.redis_game_ctx.hset("{}:{}".format(new_data["gamename"],new_data["id"]), "secretkey", new_data["secretkey"])
        self.redis_game_ctx.hset("{}:{}".format(new_data["gamename"],new_data["id"]), "description", new_data["description"])
        self.redis_game_ctx.hset("{}:{}".format(new_data["gamename"],new_data["id"]), "queryport", new_data["queryport"])
        self.redis_game_ctx.hset("{}:{}".format(new_data["gamename"],new_data["id"]), "disabled_services", new_data["disabledservices"])

    def sync_group_to_redis(self, new_data, old_data):
        new_game = Game.select().where(Game.id == new_data["gameid"]).get()
        old_game = Game.select().where(Game.id == old_data["gameid"]).get()
        self.redis_group_ctx.delete("{}:{}:".format(old_game.gamename,old_data["groupid"]))
        self.redis_group_ctx.delete("{}:{}:custkeys".format(old_game.gamename,old_data["groupid"]))

        redis_key = "{}:{}:".format(new_game.gamename,new_data["groupid"])
        self.redis_group_ctx.hset(redis_key,"gameid",new_data["gameid"])
        self.redis_group_ctx.hset(redis_key,"groupid",new_data["groupid"])
        self.redis_group_ctx.hset(redis_key,"maxwaiting",new_data["maxwaiting"])
        self.redis_group_ctx.hset(redis_key,"password",0)
        self.redis_group_ctx.hset(redis_key,"numwaiting",0)

        other_str = new_data["other"]
        other_data = other_str.split("\\")[1::]

        it = iter(other_data)
        for x in it:
            key = x
            val = next(it)
            self.redis_group_ctx.hset("{}custkeys".format(redis_key),key,val)
    def handle_get_games(self, request):
        ret = []
        where_statement = 1==1
        if "where" in request:
            where_req = request["where"]
            if "id" in where_req:
                where_statement = (where_statement) & (Game.id == int(where_req["id"]))
            elif "gamename" in where_req:
                where_statement = where_statement &(Game.gamename == where_req["gamename"])
        games = Game.select().where(where_statement)
            
        for game in games:
            ret.append(model_to_dict(game))
        return ret

    def handle_get_groups(self, request):
        ret = []
        where_statement = 1==1
        if "where" in request:
            where_req = request["where"]
            if "groupid" in where_req:
                where_statement = (where_statement) & (GameGroup.groupid == int(where_req["groupid"]))
            elif "gameid" in where_req:
                where_statement = where_statement &(GameGroup.gameid == where_req["gameid"])
        groups = GameGroup.select().where(where_statement)
            
        for group in groups:
            ret.append(model_to_dict(group))
        return ret
    def handle_update_group(self, request, create):
        group_data = request["group"]

        if create:
            extra_params = ["numplayers", "password", "numservers", "numwaiting", "updatetime"] #temporary until these are removed/placed into other
            for param in extra_params:
                if param not in group_data:
                    group_data[param] = 0
            group_pk = GameGroup.insert(**group_data).execute()
            group = GameGroup.get((GameGroup.groupid == group_pk))
        else:            
            group = GameGroup.select().where(GameGroup.groupid == group_data["groupid"]).get()

        old_data = model_to_dict(group)
        for key in group_data:
            if key != "id":
                setattr(group, key, group_data[key])
        group.save()
        self.sync_group_to_redis(model_to_dict(group), old_data)
        return {"success": True}
    def handle_update_game(self, request):
        game_data = request["game"]
        game = Game.select().where(Game.id == game_data["id"]).get()

        old_data = model_to_dict(game)
        for key in game_data:
            if key != "id":
                setattr(game, key, game_data[key])
        game.save()
        self.sync_game_to_redis(model_to_dict(game), old_data)
        return {"success": True}
    def handle_delete_group(self, request):
        group_data = request["group"]
        game = Game.select().where(Game.id == group_data["gameid"]).get()
        count = GameGroup.delete().where(GameGroup.groupid == group_data["groupid"]).execute()
        self.redis_group_ctx.delete("{}:{}:".format(game.gamename,group_data["groupid"]))
        self.redis_group_ctx.delete("{}:{}:custkeys".format(game.gamename,group_data["groupid"]))
        return {"success": True, "count": count}
    def get_server_by_key(self, server_key):
        server_info = {}
        main_keys = ["gameid", "id", "wan_port", "wan_ip", "deleted"]
        for key in main_keys:
            server_info[key] = self.redis_ctx.hget(server_key, key).decode('utf8')
        custkeys = {}
        server_info['key'] = server_key
        cursor = 0
        while True:
            resp = self.redis_ctx.hscan("{}custkeys".format(server_key),cursor)
            cursor = resp[0]
            for key, val in resp[1].items():
                custkeys[key.decode('utf8')] = val.decode('utf8')
            if cursor == 0:
                break
        server_info['custkeys'] = custkeys
        cursor = 0
        player_index = 0
        player_keys = []
        player_key_set = {}
        while True:
            player_key = "{}custkeys_player_{}".format(server_key, player_index)
            if not self.redis_ctx.exists(player_key):
                break
            while True:
                resp = self.redis_ctx.hscan(player_key,cursor)
                cursor = resp[0]
                for key, val in resp[1].items():
                    player_key_set[key.decode('utf8')] = val.decode('utf8')
                if cursor == 0:
                    break
            player_keys.append(player_key_set)
            player_key_set = {}
            player_index = player_index + 1
        server_info['player_custkeys'] = player_keys


        cursor = 0
        team_index = 0
        team_keys = []
        team_key_set = {}
        while True:
            team_key = "{}custkeys_team_{}".format(server_key, team_index)
            if not self.redis_ctx.exists(team_key):
                break
            while True:
                resp = self.redis_ctx.hscan(team_key,cursor)
                cursor = resp[0]
                for key, val in resp[1].items():
                    team_key_set[key.decode('utf8')] = val.decode('utf8')
                if cursor == 0:
                    break
            team_keys.append(team_key_set)
            team_key_set = {}
            team_index = team_index + 1
        server_info['team_custkeys'] = team_keys
        return server_info
    def handle_get_servers(self, request):
        where_params = {}
        msg_scan_key = "*:*:*:"
        if "where" in request:
            where_params = request["where"]
            if "gamename" in where_params:
                msg_scan_key = "{}:*:*:".format(where_params["gamename"])
            elif "id" in where_params:
                msg_scan_key = "*:*:{}:".format(where_params["id"])
            elif "key" in where_params:
                server = self.get_server_by_key(where_params["key"])
                return {"servers": [server]}
        cursor = 0

        servers = []
        while True:
            resp = self.redis_ctx.scan(cursor, msg_scan_key)
            cursor = resp[0]
            for item in resp[1]:
                key = item.decode('utf8')
                servers.append(self.get_server_by_key(key))
            if cursor == 0:
                break
        return {"servers": servers}
    def handle_update_server(self, request):
        return True
    def handle_delete_server(self, request):
        if "key" in request:
            self.redis_ctx.hset(request["key"],"deleted", "1")
        return {"success": True}
    def run(self, env, start_response):
        # the environment variable CONTENT_LENGTH may be empty or missing
        try:
            request_body_size = int(env.get('CONTENT_LENGTH', 0))
        except ValueError:
            request_body_size = 0


        response = {}
        response['success'] = False

        # When the method is POST the variable will be sent
        # in the HTTP request body which is passed by the WSGI server
        # in the file like wsgi.input environment variable.
        request_body = json.loads(env['wsgi.input'].read(request_body_size).decode('utf-8'))

        start_response('200 OK', [('Content-Type','application/json')])

        if "type" in request_body:
            if request_body["type"] == "get_games":
                game_data = self.handle_get_games(request_body)
                response['success'] = True
                response["data"] = game_data
            elif request_body["type"] == "get_groups":
                group_data = self.handle_get_groups(request_body)
                response['success'] = True
                response["data"] = group_data
            elif request_body["type"] == "update_game":
                self.handle_update_game(request_body)
                response['success'] = True
            elif request_body["type"] == "update_group":
                self.handle_update_group(request_body, False)
                response['success'] = True
            elif request_body["type"] == "delete_group":
                self.handle_delete_group(request_body)
                response['success'] = True
            elif request_body["type"] == "create_group":
                self.handle_update_group(request_body, True)
                response['success'] = True
            elif request_body["type"] == "get_servers":
                response = self.handle_get_servers(request_body)
                response['success'] = True
            elif request_body["type"] == "update_server":
                response = self.handle_update_server(request_body)
                response['success'] = True
            elif request_body["type"] == "delete_server":
                response = self.handle_delete_server(request_body)
                response['success'] = True

        return response
