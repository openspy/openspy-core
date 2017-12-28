from cgi import parse_qs, escape

import uuid
import json

from playhouse.shortcuts import model_to_dict, dict_to_model
from BaseModel import BaseModel
from Model.User import User
from Model.Profile import Profile
from Model.Buddy import Buddy
from Model.Block import Block

from BaseService import BaseService
import json
import uuid
import redis
import os

import time

class UserProfileMgrService(BaseService):
    def __init__(self):
        BaseService.__init__(self)
        self.valid_characters = "1234567890#_-`()$-=;/@+&abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
        self.redis_presence_channel = "presence.buddies"
        self.redis_presence_ctx = redis.StrictRedis(host=os.environ['REDIS_SERV'], port=int(os.environ['REDIS_PORT']), db = 5)

    def handle_update_profile(self, data):

        if "profile" not in data:
            data = {'profile': data}
        if "profileid" in data["profile"]:
            data["profile"]["id"] = data["profile"]["profileid"]
            del data["profile"]["profileid"]
        if "nick" in data["profile"]:
            data["profile"]["nick"] = data["profile"]["nick"]
            del data["profile"]["nick"]

        profile_model = Profile.get((Profile.id == data['profile']['id']))
        namespaceid = 0
        if "namespaceid" in data['profile']:
            namespaceid = data['profile']['namespaceid']
        if "nick" in data['profile']:
            if not self.is_name_valid(data['profile']['nick']):
                return {'error': 'NICK_INVALID'}
        if "uniquenick" in data['profile']:
            if not self.is_name_valid(data['profile']['uniquenick']):
                return {'error': 'UNIQUENICK_INVALID'}
            if data['profile']['uniquenick'] != data["profile"]["uniquenick"] and not self.check_uniquenick_available(data['profile']['uniquenick'], namespaceid)["exists"]:
                return {'error': 'UNIQUENICK_INUSE'}
        for key in data['profile']:
            if key != "id":
                setattr(profile_model, key, data['profile'][key])
        profile_model.save()
        return {"success": True}
    def is_name_valid(self, name):
        for n in name:
            if n not in self.valid_characters:
                return False
        return True
    def handle_get_profiles(self, data):
        profiles = []
        try:
            for profile in Profile.select().join(User).where((User.id == data["userid"]) & (Profile.deleted == False)):
                profile_dict = model_to_dict(profile)
                del profile_dict['user']
                profiles.append(profile_dict)

        except Profile.DoesNotExist:
            return []
        return profiles


    def handle_get_profile(self, data):
        profile = None
        try:
            if "profileid" in data:
                profile = Profile.get((Profile.id == data["profileid"]))
            elif "uniquenick" in data:
                if "namespaceid" in data:
                    profile = Profile.get((Profile.uniquenick == data["uniquenick"]) & (Profile.namespaceid == data["namespaceid"]))
                else:
                    profile = Profile.get((Profile.uniquenick == data["uniquenick"]) & (Profile.namespaceid == 0))

                return model_to_dict(profile)
        except Profile.DoesNotExist:
            return None


    def check_uniquenick_available(self, uniquenick, namespaceid):
        try:
            if namespaceid != 0:
                profile = Profile.get((Profile.uniquenick == uniquenick) & (Profile.namespaceid == namespaceid) & (Profile.deleted == False))
            else:
                profile = Profile.get((Profile.uniquenick == uniquenick) & (Profile.deleted == False))

                profile = model_to_dict(profile)
                del profile["user"]["password"]
            return {"exists": True, "profile": profile}
        except Profile.DoesNotExist:
            return {"exists": False}
    def handle_create_profile(self, data):
        profile_data = data["profile"]
        user_data = None
        if "user" in data:
            user_data = data["user"]
        if "namespaceid" in profile_data:
                namespaceid = profile_data["namespaceid"]
        else:
            namespaceid = 0
        if "uniquenick" in profile_data:
            if not self.is_name_valid(profile_data["uniquenick"]):
                return {'error': 'INVALID_UNIQUENICK'}
            nick_available = self.check_uniquenick_available(profile_data["uniquenick"], namespaceid)
            if nick_available["exists"]:
                return {'error': 'UNIQUENICK_IN_USE', "profile": nick_available["profile"]}
        user = User.get((User.id == user_data["id"]))
        if "nick" in profile_data:
            if not self.is_name_valid(profile_data["nick"]):
                return {'error': 'INVALID_NICK'}
        profile_data["user"] = user
        profile_pk = Profile.insert(**profile_data).execute()
        profile = Profile.get((Profile.id == profile_pk))
        profile = model_to_dict(profile)
        del profile["user"]
        return profile
    def handle_delete_profile(self, data):
        profile_data = data["profile"]
        try:
            profile = Profile.get((Profile.id == profile_data["id"]))
            if profile.deleted:
                return False
            profile.deleted = True
            profile.save()
        except Profile.DoesNotExist:
            return False
        return True

    def handle_profile_search(self, search_data):
        response = []

        #{u'chc': 0, u'ooc': 0, u'i1': 0, u'pic': 0, u'lon': 0.0, u'mar': 0, u'namespaceids': [1], u'lat': 0.0,
        #u'birthday': 0, u'mode': u'profile_search', u'partnercode': 0, u'ind': 0, u'sex': 0, u'email': u'sctest@gamespy.com'}

        profile_search_data = search_data["profile"]
        user_seach_data = search_data["user"]

        where_expression = ((Profile.deleted == False) & (User.deleted == False))

        #user search
        if "userid" in profile_search_data:
            where_expression = ((where_expression) & (User.id == profile_search_data["userid"]))
        elif "id" in user_seach_data:
            where_expression = ((where_expression) & (User.id == user_seach_data["id"]))

        if "email" in user_seach_data:
            where_expression = ((where_expression) & (User.email == user_seach_data["email"]))

        if "partnercode" in profile_search_data:
            where_expression = ((where_expression) & (User.partnercode == profile_search_data["partnercode"]))

        #profile search
        if "id" in profile_search_data:
            where_expression = ((where_expression) & (Profile.id == profile_search_data["id"]))

        if "nick" in profile_search_data:
            where_expression = ((where_expression) & (Profile.nick == profile_search_data["nick"]))

        if "uniquenick" in profile_search_data:
            where_expression = ((where_expression) & (Profile.uniquenick == profile_search_data["uniquenick"]))

        if "firstname" in profile_search_data:
            where_expression = ((where_expression) & (Profile.firstname == profile_search_data["firstname"]))

        if "lastname" in profile_search_data:
            where_expression = ((where_expression) & (Profile.firstname == profile_search_data["lastname"]))

        if "icquin" in profile_search_data:
            where_expression = ((where_expression) & (Profile.icquin == profile_search_data["icquin"]))

        if "namespaceids" in profile_search_data:
            namespace_expression = None
            for namespaceid in profile_search_data["namespaceids"]:
                if namespace_expression == None:
                    namespace_expression = (Profile.namespaceid == namespaceid)
                else:
                    namespace_expression = ((Profile.namespaceid == namespaceid) | (namespace_expression))
            if namespace_expression != None:
                where_expression = (where_expression) & (namespace_expression)

        profiles = Profile.select().join(User).where(where_expression)
        ret_profiles = []
        for profile in profiles:
            append_profile = model_to_dict(profile)
            append_profile['userid'] = append_profile['user']['id']
            del append_profile['user']['password']

            ret_profiles.append(append_profile)

        return ret_profiles

    def handle_authorize_buddy_add(self, request_data):
        success = False
        redis_key = "add_req_{}".format(request_data["from_profileid"])
        if self.redis_presence_ctx.exists(redis_key):
            if self.redis_presence_ctx.hexists(redis_key, request_data["to_profileid"]):
                if self.redis_presence_ctx.hdel(redis_key, request_data["to_profileid"]):
                    success = True
                    publish_data = "\\type\\authorize_add\\from_profileid\\{}\\to_profileid\\{}".format(request_data["from_profileid"], request_data["to_profileid"])
                    self.redis_presence_ctx.publish(self.redis_presence_channel, publish_data)
                    to_profile_model = Profile.get((Profile.id == request_data["to_profileid"]))
                    from_profile_model = Profile.get((Profile.id == request_data["from_profileid"]))
                    Buddy.insert(from_profile=to_profile_model,to_profile=from_profile_model).execute()
        return success

    def handle_del_buddy(self, request_data):
        success = False
        from_profile = Profile.get((Profile.id == request_data["from_profileid"]) & (Profile.deleted == False))
        to_profile = Profile.get((Profile.id == request_data["to_profileid"]) & (Profile.deleted == False))
        send_revoke = "send_revoke" in request_data and request_data["send_revoke"]
        count = Buddy.delete().where((Buddy.from_profile == from_profile) & (Buddy.to_profile == to_profile)).execute()
        if count > 0:
            success = True
            if send_revoke:
                status_key = "status_{}".format(to_profile)
                if self.redis_presence_ctx.exists(status_key): #check if online
                    #send revoke to GP
                    publish_data = "\\type\\del_buddy\\from_profileid\\{}\\to_profileid\\{}".format(request_data["from_profileid"], request_data["to_profileid"])
                    self.redis_presence_ctx.publish(self.redis_presence_channel, publish_data)
                else:
                    revoke_list_key = "revokes_{}".format(request_data["to_profileid"])
                    timestamp = int(time.time())
                    self.redis_presence_ctx.hset(revoke_list_key,request_data["from_profileid"],timestamp)
        return success
    #publish new deletes, pending msgs, etc to presence for when a user logged in, and delete
    def handle_send_presence_login_messages(self, request_data):
        profile = Profile.get((Profile.id == request_data["profileid"]) & (Profile.deleted == False))
        #send revokes
        revoke_list_key = "revokes_{}".format(profile.id)

        cursor = 0
        while True:
            resp = self.redis_presence_ctx.hscan(revoke_list_key, cursor)
            cursor = resp[0]
            for pid, time in resp[1].items():
                    publish_data = "\\type\\del_buddy\\from_profileid\\{}\\to_profileid\\{}".format(pid, profile.id)
                    self.redis_presence_ctx.publish(self.redis_presence_channel, publish_data)
            self.redis_presence_ctx.delete(revoke_list_key)
            if cursor == 0:
                break


        #send add requests
        add_req_key = "add_req_{}".format(profile.id)
        cursor = 0
        while True:
            resp = self.redis_presence_ctx.hscan(add_req_key, cursor)
            cursor = resp[0]
            for pid, reason in resp[1].items():
                publish_data = "\\type\\add_request\\from_profileid\\{}\\to_profileid\\{}\\reason\\{}".format(int(pid.decode("utf-8")), profile.id, reason.decode("utf-8"))
                self.redis_presence_ctx.publish(self.redis_presence_channel, publish_data)
            if cursor == 0:
                break

        #send pending msgs
        msg_scan_key = "msg_{}_*".format(profile.id)
        cursor = 0
        while True:
            resp = self.redis_presence_ctx.scan(cursor, msg_scan_key)
            cursor = resp[0]
            for key in resp[1]:
                msg_key = key
                message = self.redis_presence_ctx.hget(msg_key, "message").decode("utf-8")
                timestamp = int(self.redis_presence_ctx.hget(msg_key, "timestamp").decode("utf-8"))
                msg_type = int(self.redis_presence_ctx.hget(msg_key, "type").decode("utf-8"))
                msg_from = int(self.redis_presence_ctx.hget(msg_key, "from").decode("utf-8"))
                self.redis_presence_ctx.delete(msg_key)
                publish_data = "\\type\\buddy_message\\from_profileid\\{}\\to_profileid\\{}\\msg_type\\{}\\message\\{}".format(msg_from, profile.id, msg_type, message)
                self.redis_presence_ctx.publish(self.redis_presence_channel, publish_data)
            if cursor == 0:
                break
        return True

    def get_gp_status(self, profileid):
        redis_key = "status_{}".format(profileid)
        status_keys = ["status", "status_string", "location_string", "quiet_flags", "ip", "port"]
        int_fields = ["status", "quiet_flags", "port"]
        ret = {}
        if self.redis_presence_ctx.exists(redis_key):
            for key in status_keys:
                ret[key] = self.redis_presence_ctx.hget(redis_key, key)
                if key in int_fields and key != None:
                    if key in ret and ret[key] != None:
                        ret[key] = int(ret[key])
        else:
            for key in status_keys:
                ret[key] = None

        return ret
    def handle_get_buddies_status(self, request_data):
        ret = []
        try:
            profile = Profile.get(Profile.id == request_data["profileid"])
            buddies = Buddy.select().where((Buddy.from_profile == profile))
            for buddy in buddies:
                status_data = self.get_gp_status(buddy.to_profile.id)
                status_data['profileid'] = buddy.to_profile.id
                ret.append(status_data)
        except Buddy.DoesNotExist:
            return []
        except Profile.DoesNotExist:
            return []

        return ret

    def handle_get_blocks_status(self, request_data):
        ret = []
        try:
            profile = Profile.get(Profile.id == request_data["profileid"])
            blocks = Block.select().where((Block.from_profile == profile))
            for block in blocks:
                status_data = self.get_gp_status(block.to_profile.id)
                status_data['profileid'] = block.to_profile.id
                ret.append(status_data)
        except Block.DoesNotExist:
            return []
        except Profile.DoesNotExist:
            return []

        return ret
    #Get the buddies for a profile.
    def handle_buddies_search(self, request_data):
        profile_data = request_data["profile"]
        if "id" not in profile_data:
            return False

        profile = Profile.get(Profile.id == profile_data["id"])

        buddies = Buddy.select().where((Buddy.from_profile == profile))
        ret = []
        try:
            for buddy in buddies:
                model = model_to_dict(buddy)
                to_profile = model['to_profile']
                del to_profile['user']['password']
                #to_profile['gp_status'] = self.get_gp_status(to_profile['id'])
                ret.append(to_profile)
        except Profile.DoesNotExist:
            return []
        except Buddy.DoesNotExist:
            return []
        return ret

    def handle_blocks_search(self, request_data):
        profile_data = request_data["profile"]
        if "id" not in profile_data:
            return False

        profile = Profile.get(Profile.id == profile_data["id"])

        buddies = Block.select().where((Block.from_profile == profile))
        ret = []
        try:
            for buddy in buddies:
                model = model_to_dict(buddy)
                to_profile = model['to_profile']
                del to_profile['user']['password']
                ret.append(to_profile)
        except Profile.DoesNotExist:
            return []
        except Buddy.DoesNotExist:
            return []
        return ret

    #Get a list of profiles that have the specified profiles as buddies.
    def handle_reverse_buddies_search(self, request_data):
        ret = []
        try:
            buddies = Buddy.select().where((Buddy.from_profile << request_data["target_profileids"])).execute()
            for buddy in buddies:
                model = model_to_dict(buddy)
                to_profile = model['to_profile']
                del to_profile['user']['password']
                ret.append(to_profile)

        except Profile.DoesNotExist:
            return []
        except Buddy.DoesNotExist:
            return []
        return ret

    def send_buddy_message(self, request_data):
        try:
            from_profile = Profile.get((Profile.id == request_data["from_profileid"]) & (Profile.deleted == False))
            to_profile = Profile.get((Profile.id == request_data["to_profileid"]) & (Profile.deleted == False))

            status_key = "status_{}".format(to_profile.id)
            if self.redis_presence_ctx.exists(status_key): #check if online
                publish_data = "\\type\\buddy_message\\from_profileid\\{}\\to_profileid\\{}\\msg_type\\{}\\message\\{}".format(from_profile.id, to_profile.id, request_data["type"], request_data["message"])
                self.redis_presence_ctx.publish(self.redis_presence_channel, publish_data)
            else:
                msg_id = self.redis_presence_ctx.incr("MSG_CNT")
                redis_key = "msg_{}_{}".format(to_profile.id, msg_id)
                self.redis_presence_ctx.hset(redis_key, "message", request_data["message"])
                self.redis_presence_ctx.hset(redis_key, "timestamp", int(time.time()))
                self.redis_presence_ctx.hset(redis_key, "type", request_data["type"])
                self.redis_presence_ctx.hset(redis_key, "from", from_profile.id)
        except Profile.DoesNotExist:
            return False
        return True
    def handle_block_buddy(self, request_data):
        try:
            to_profile_model = Profile.get((Profile.id == request_data["to_profileid"]))
            from_profile_model = Profile.get((Profile.id == request_data["from_profileid"]))

            success = Block.insert(to_profile=to_profile_model,from_profile=from_profile_model).execute() != 0
            if success:
                publish_data = "\\type\\block_buddy\\from_profileid\\{}\\to_profileid\\{}\\".format(from_profile_model.id, to_profile_model.id)
                self.redis_presence_ctx.publish(self.redis_presence_channel, publish_data)
        except Profile.DoesNotExist:
            return False
    def handle_del_block_buddy(self, request_data):
        try:
            to_profile_model = Profile.get((Profile.id == request_data["to_profileid"]))
            from_profile_model = Profile.get((Profile.id == request_data["from_profileid"]))
            count = Block.delete().where((Block.from_profile == from_profile_model) & (Block.to_profile == to_profile_model)).execute()
            if count > 0:
                publish_data = "\\type\\del_block_buddy\\from_profileid\\{}\\to_profileid\\{}\\".format(from_profile_model.id, to_profile_model.id)
                self.redis_presence_ctx.publish(self.redis_presence_channel, publish_data)
                return True
        except Profile.DoesNotExist:
            return False
        return False
    def run(self, env, start_response):
        # the environment variable CONTENT_LENGTH may be empty or missing
        try:
            request_body_size = int(env.get('CONTENT_LENGTH', 0))
        except (ValueError):
            request_body_size = 0

        response = {}
        response['success'] = False

        # When the method is POST the variable will be sent
        # in the HTTP request body which is passed by the WSGI server
        # in the file like wsgi.input environment variable.
        request_body = json.loads(env['wsgi.input'].read(request_body_size).decode('utf-8'))

        response = {}

        success = False

        if "mode" not in request_body:
            response['error'] = "INVALID_MODE"
            return response

        if request_body["mode"] == "update_profile":
            resp = self.handle_update_profile(request_body)
            if "error" in resp:
                response['error'] = resp["error"]
            else:
                success = True
        elif request_body["mode"] == "get_profile":
            profile = self.handle_get_profile(request_body)
            success = profile != None
            response['profile'] = profile
        elif request_body["mode"] == "get_profiles":
            profiles = self.handle_get_profiles(request_body)
            success = True
            response['profiles'] = profiles
        elif request_body["mode"] == "create_profile":
            profile = self.handle_create_profile(request_body)
            if "error" in profile:
                response['error'] = profile['error']
                if "profile" in profile:
                    response["profile"] = profile["profile"]
            elif profile != None:
                success = True
                response['profile'] = profile
        elif request_body["mode"] == "delete_profile":
            success = self.handle_delete_profile(request_body)
        elif request_body["mode"] == "profile_search":
            profiles = self.handle_profile_search(request_body)
            response['profiles'] = profiles
            success = True
        elif request_body["mode"] == "authorize_buddy_add":
            success = self.handle_authorize_buddy_add(request_body)
        elif request_body["mode"] == "buddies_search":
            profiles = self.handle_buddies_search(request_body)
            response['profiles'] = profiles
            success = True
        elif request_body["mode"] == "buddies_reverse_search": #get who has who on given profileid
            profiles = self.handle_reverse_buddies_search(request_body)
            response['profiles'] = profiles
            success = True
        elif request_body["mode"] == "blocks_search":
            profiles = self.handle_blocks_search(request_body)
            response['profiles'] = profiles
            success = True
        elif request_body["mode"] == "del_buddy":
            success = self.handle_del_buddy(request_body)
        elif request_body["mode"] == "get_buddies_revokes":
            revokes = self.handle_get_buddy_revokes(request_body)
            response['revokes'] = revokes
            success = True
        elif request_body["mode"] == "send_presence_login_messages":
            self.handle_send_presence_login_messages(request_body)
            success = True
        elif request_body["mode"] == "send_buddy_message":
            revokes = self.send_buddy_message(request_body)
            response['revokes'] = revokes
            success = True
        elif request_body["mode"] == "block_buddy":
            success = self.handle_block_buddy(request_body)
        elif request_body["mode"] == "del_block_buddy":
            success = self.handle_del_block_buddy(request_body)
        elif request_body["mode"] == "get_buddies_status":
            statuses = self.handle_get_buddies_status(request_body)
            response['statuses'] = statuses
            success = True
        elif request_body["mode"] == "get_blocks_status":
            statuses = self.handle_get_blocks_status(request_body)
            response['statuses'] = statuses
            success = True

        response['success'] = success
        start_response('200 OK', [('Content-Type','application/json')])
        return response
