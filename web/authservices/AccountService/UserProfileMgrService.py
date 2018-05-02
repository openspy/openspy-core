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

from lib.Exceptions.OS_BaseException import OS_BaseException
from lib.Exceptions.OS_CommonExceptions import *

class UserProfileMgrService(BaseService):
    def __init__(self):
        BaseService.__init__(self)
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
            if not Profile.is_nick_valid(data['profile']['nick'], namespaceid):
                raise OS_Profile_NickInvalid()
        if "uniquenick" in data['profile']:
            if not Profile.is_nick_valid(data['profile']['uniquenick'], namespaceid):
                raise OS_Profile_UniquenickInvalid()
            nick_available = self.check_uniquenick_available(data['profile']['uniquenick'], namespaceid)
            if nick_available["exists"]:
                raise OS_Profile_UniquenickInUse(nick_available["profile"])
        for key in data['profile']:
            if key != "id":
                setattr(profile_model, key, data['profile'][key])
        profile_model.save()
        return {"success": True}
    def handle_get_profiles(self, data):
        response = {"success": False}
        profiles = []
        try:
            for profile in Profile.select().where((Profile.userid == data["userid"]) & (Profile.deleted == False)):
                profile_dict = model_to_dict(profile)
                if "user" in profile_dict:
                    del profile_dict['user']['password']
                profiles.append(profile_dict)
            response["success"] = True
        except Profile.DoesNotExist:
            raise OS_Auth_NoSuchUser()

        response["profiles"] = profiles
        return response


    def handle_get_profile(self, data):
        profile = None
        response = {}
        try:
            if "profileid" in data:
                profile = Profile.get((Profile.id == data["profileid"]))
            elif "uniquenick" in data:
                if "namespaceid" in data:
                    profile = Profile.get((Profile.uniquenick == data["uniquenick"]) & (Profile.namespaceid == data["namespaceid"]))
                else:
                    profile = Profile.get((Profile.uniquenick == data["uniquenick"]) & (Profile.namespaceid == 0))
            if profile:
                response["profile"] = model_to_dict(profile)
                if "user" in response["profile"]:
                    del response["profile"]["user"]
                response["success"] = True
        except Profile.DoesNotExist:
            pass

        return response


    def check_uniquenick_available(self, uniquenick, namespaceid):
        try:
            if namespaceid != 0:
                profile = Profile.get((Profile.uniquenick == uniquenick) & (Profile.namespaceid == namespaceid) & (Profile.deleted == False))
                return {"exists": True, "profile": profile}
            else:
                #profile = Profile.get((Profile.uniquenick == uniquenick) & (Profile.deleted == False))

                #profile = model_to_dict(profile)
                #del profile["user"]["password"]
                return {"exists": False}
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
            if not Profile.is_nick_valid(profile_data["uniquenick"], namespaceid):
                raise OS_Profile_UniquenickInvalid()
            nick_available = self.check_uniquenick_available(profile_data["uniquenick"], namespaceid)
            if nick_available["exists"]:
                raise OS_Profile_UniquenickInUse(nick_available["profile"])
        user = User.get((User.id == user_data["id"]))
        if "nick" in profile_data:
            if not Profile.is_nick_valid(profile_data["nick"], namespaceid):
                raise OS_Profile_NickInvalid()
        profile_data["user"] = user
        profile_pk = Profile.insert(**profile_data).execute()
        profile = Profile.get((Profile.id == profile_pk))
        profile = model_to_dict(profile)
        del profile["user"]
        user = model_to_dict(user)
        return {"user": user, "profile": profile, "success": True}
    def handle_delete_profile(self, data):
        profile_data = data["profile"]
        response = {"success": False}
        try:
            profile = Profile.get((Profile.id == profile_data["id"]))
            if profile.deleted:
                return response
            profile.deleted = True
            profile.save()
            response["success"] = True
        except Profile.DoesNotExist:
            pass
        return response

    def handle_profile_search(self, search_data):

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

        return {"profiles":ret_profiles, "success": True}

    def handle_authorize_buddy_add(self, request_data):
        success = False
        redis_key = "add_req_{}".format(request_data["from_profileid"])
        if self.redis_presence_ctx.exists(redis_key):
            if self.redis_presence_ctx.hexists(redis_key, request_data["to_profileid"]):
                if self.redis_presence_ctx.hdel(redis_key, request_data["to_profileid"]):
                    success = True

                    to_profile_model = Profile.get((Profile.id == request_data["to_profileid"]))
                    from_profile_model = Profile.get((Profile.id == request_data["from_profileid"]))
                    Buddy.insert(from_profile=to_profile_model,to_profile=from_profile_model).execute()
                    Buddy.insert(to_profile=to_profile_model,from_profile=from_profile_model).execute()

                    publish_data = "\\type\\authorize_add\\from_profileid\\{}\\to_profileid\\{}".format(request_data["from_profileid"], request_data["to_profileid"])
                    self.redis_presence_ctx.publish(self.redis_presence_channel, publish_data)

                    publish_data = "\\type\\authorize_add\\from_profileid\\{}\\to_profileid\\{}\\silent\\1".format(request_data["to_profileid"], request_data["from_profileid"])
                    self.redis_presence_ctx.publish(self.redis_presence_channel, publish_data)
        return {"success": success}

    def handle_del_buddy(self, request_data):
        response = {"success": False}
        success = False
        from_profile = Profile.get((Profile.id == request_data["from_profileid"]) & (Profile.deleted == False))
        to_profile = Profile.get((Profile.id == request_data["to_profileid"]) & (Profile.deleted == False))
        send_revoke = "send_revoke" in request_data and request_data["send_revoke"]
        count = Buddy.delete().where((Buddy.from_profile == from_profile) & (Buddy.to_profile == to_profile)).execute()
        if count > 0:
            response["success"] = True
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
        return {"success": True}

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
        response = {"statuses": [], "success": False}
        try:
            profile = Profile.get(Profile.id == request_data["profileid"])
            buddies = Buddy.select().where((Buddy.from_profile == profile))
            for buddy in buddies:
                status_data = self.get_gp_status(buddy.to_profile.id)
                status_data['profileid'] = buddy.to_profile.id
                response["statuses"].append(status_data)
            response["success"] = True
        except (Buddy.DoesNotExist, Profile.DoesNotExist) as e:
            pass

        return response

    def handle_get_blocks_status(self, request_data):
        response = {"profiles": []}
        try:
            profile = Profile.get(Profile.id == request_data["profileid"])
            blocks = Block.select().where((Block.from_profile == profile))
            for block in blocks:
                status_data = self.get_gp_status(block.to_profile.id)
                status_data['profileid'] = block.to_profile.id
                response["profiles"].append(status_data)
        except (Block.DoesNotExist, Profile.DoesNotExist) as e:
            pass
        return response
    #Get the buddies for a profile.
    def handle_buddies_search(self, request_data):
        response = {"profiles": [], "success": False}
        profile_data = request_data["profile"]
        if "id" not in profile_data:
            return response

        profile = Profile.get(Profile.id == profile_data["id"])

        buddies = Buddy.select().where((Buddy.from_profile == profile))
        try:
            for buddy in buddies:
                model = model_to_dict(buddy)
                to_profile = model['to_profile']
                del to_profile['user']['password']
                #to_profile['gp_status'] = self.get_gp_status(to_profile['id'])
                response["profiles"].append(to_profile)
                response["success"] = True
        except (Profile.DoesNotExist, Buddy.DoesNotExist) as e:
            pass
        return response

    def handle_blocks_search(self, request_data):
        response = {"profiles": [], "success": False}
        profile_data = request_data["profile"]
        if "id" not in profile_data:
            return False

        profile = Profile.get(Profile.id == profile_data["id"])

        buddies = Block.select().where((Block.from_profile == profile))
        try:
            for buddy in buddies:
                model = model_to_dict(buddy)
                to_profile = model['to_profile']
                del to_profile['user']['password']
                response["profiles"].append(to_profile)
                response["success"] = True
        except (Buddy.DoesNotExist, Profile.DoesNotExist) as e:
            pass
        return response

    #Get a list of profiles that have the specified profiles as buddies.
    def handle_reverse_buddies_search(self, request_data):
        response = {"profiles": [], "success": False}
        try:
            buddies = Buddy.select().where((Buddy.from_profile << request_data["target_profileids"])).execute()
            for buddy in buddies:
                model = model_to_dict(buddy)
                to_profile = model['to_profile']
                del to_profile['user']['password']
                response["profiles"].append(to_profile)
                response["success"] = True
        except (Profile.DoesNotExist, Buddy.DoesNotExist) as e:
            pass
        return response

    def send_buddy_message(self, request_data):
        response = {"success": False}
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
                response["success"] = True
        except Profile.DoesNotExist:
            pass
        return response
    def handle_block_buddy(self, request_data):
        response = {"success": False}
        try:
            to_profile_model = Profile.get((Profile.id == request_data["to_profileid"]))
            from_profile_model = Profile.get((Profile.id == request_data["from_profileid"]))

            success = Block.insert(to_profile=to_profile_model,from_profile=from_profile_model).execute() != 0
            if success:
                publish_data = "\\type\\block_buddy\\from_profileid\\{}\\to_profileid\\{}\\".format(from_profile_model.id, to_profile_model.id)
                self.redis_presence_ctx.publish(self.redis_presence_channel, publish_data)
            response["success"] = True
        except Profile.DoesNotExist:
            pass
        return response
    def handle_del_block_buddy(self, request_data):
        response = {"success": False}
        try:
            to_profile_model = Profile.get((Profile.id == request_data["to_profileid"]))
            from_profile_model = Profile.get((Profile.id == request_data["from_profileid"]))
            count = Block.delete().where((Block.from_profile == from_profile_model) & (Block.to_profile == to_profile_model)).execute()
            if count > 0:
                publish_data = "\\type\\del_block_buddy\\from_profileid\\{}\\to_profileid\\{}\\".format(from_profile_model.id, to_profile_model.id)
                self.redis_presence_ctx.publish(self.redis_presence_channel, publish_data)
            response["success"] = True
        except Profile.DoesNotExist:
            pass
        return response
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

        start_response('200 OK', [('Content-Type','application/json')])

        mode_table = {
            "update_profile": self.handle_update_profile,
            "get_profile": self.handle_get_profile,
            "get_profiles": self.handle_get_profiles,
            "create_profile": self.handle_create_profile,
            "delete_profile": self.handle_delete_profile,
            "profile_search": self.handle_profile_search,
            "authorize_buddy_add": self.handle_authorize_buddy_add,
            "buddies_search": self.handle_buddies_search,
            "buddies_reverse_search": self.handle_reverse_buddies_search,
            "blocks_search": self.handle_blocks_search,
            "del_buddy": self.handle_del_buddy,
            #"get_buddies_revokes": self.handle_get_buddy_revokes,
            "send_presence_login_messages": self.handle_send_presence_login_messages,
            "send_buddy_message": self.send_buddy_message,
            "block_buddy": self.handle_block_buddy,
            "del_block_buddy": self.handle_del_block_buddy,
            "get_buddies_status": self.handle_get_buddies_status,
            "get_blocks_status": self.handle_get_blocks_status
        }

        try:
            if "mode" in request_body:
                req_type = request_body["mode"]
                if req_type in mode_table:
                    response = mode_table[req_type](request_body)
                else:
                    raise OS_InvalidMode()
        except OS_BaseException as e:
            response = e.to_dict()
        except Exception as error:
            response = {"error": repr(error)}
        return response