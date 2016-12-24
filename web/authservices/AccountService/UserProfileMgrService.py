from cgi import parse_qs, escape

import jwt

import MySQLdb
import uuid

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

import time

class UserProfileMgrService(BaseService):
    def __init__(self):
        BaseService.__init__(self)
        self.redis_presence_channel = "presence.buddies"
        self.redis_presence_ctx = redis.StrictRedis(host='localhost', port=6379, db = 5)
        
    def handle_update_profile(self, data):
        profile_model = Profile.get((Profile.id == data['profile']['id']))
        for key in data['profile']:
            if key != "id":
                setattr(profile_model, key, data['profile'][key])

        profile_model.save()
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

                return profile
        except Profile.DoesNotExist:
            return None
        

    def check_uniquenick_available(self, uniquenick, namespaceid):
        try:
            if namespaceid != 0:
                profile = Profile.get((Profile.uniquenick == uniquenick) & (Profile.namespaceid == namespaceid) & (Profile.deleted == False))
            else:
                profile = Profile.get((Profile.uniquenick == uniquenick) & (Profile.deleted == False))
            return False
        except Profile.DoesNotExist:
            return True
    def handle_create_profile(self, data):
        profile_data = data["profile"]
        if "uniquenick" in profile_data:
            namespaceid = 0
            if "namespaceid" in profile_data:
                namespaceid = profile_data["namespaceid"]
            nick_available = self.check_uniquenick_available(profile_data["uniquenick"], namespaceid)
            if not nick_available:
                return None
        user = User.get((User.id == data["userid"]))
        profile_data["user"] = user
        profile_pk = Profile.insert(**profile_data).execute()
        profile = Profile.get((Profile.id == profile_pk))
        profile = model_to_dict(profile)
        del profile["user"]
        return profile
    def handle_delete_profile(self, data):
        try:
            profile = Profile.get((Profile.id == data["profileid"]))
            if profile.deleted:
                return False
            profile.deleted = True
            profile.save()
        except Profile.DoesNotExist:
            return False
        return True

    def handle_profile_search(self, search_data):
        response = []
        #{u'partnercode': 0, u'profilenick': u'sctest01', u'email': u'sctest@gamespy.com', u'namespaceids': [1], u'mode': u'profile_search'}
        #query = Profile.select().join(User)

        hidden_str = "[hidden]"

        where_expression = ((Profile.deleted == False) & (User.deleted == False))

        #user search
        if "userid" in search_data:
            where_expression = ((where_expression) & (User.id == search_data["userid"]))
            
        if "email" in search_data:
            where_expression = ((where_expression) & (User.email == search_data["email"]))
            
        if "partnercode" in search_data:
            where_expression = ((where_expression) & (User.partnercode == search_data["partnercode"]))

        #profile search
        if "profileid" in search_data:
            where_expression = ((where_expression) & (Profile.id == search_data["profileid"]))
        if "profilenick" in search_data:
            where_expression = ((where_expression) & (Profile.nick == search_data["profilenick"]))

        if "uniquenick" in search_data:
            where_expression = ((where_expression) & (Profile.uniquenick == search_data["uniquenick"]))

        if "firstname" in search_data:
            where_expression = ((where_expression) & (Profile.firstname == search_data["firstname"]))

        if "lastname" in search_data:
            where_expression = ((where_expression) & (Profile.firstname == search_data["lastname"]))

        if "icquin" in search_data:
            where_expression = ((where_expression) & (Profile.icquin == search_data["icquin"]))

        if "namespaceids" in search_data:
            namespace_expression = None
            for namespaceid in search_data["namespaceids"]:
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
        redis_key = "add_req_{}".format(request_data["to_profileid"])
        if self.redis_presence_ctx.exists(redis_key):
            if self.redis_presence_ctx.hexists(redis_key, request_data["from_profileid"]):
                if self.redis_presence_ctx.hdel(redis_key, request_data["from_profileid"]):
                    success = True
                    publish_data = "\\type\\authorize_add\\from_profileid\\{}\\to_profileid\\{}".format(request_data["from_profileid"], request_data["to_profileid"])
                    self.redis_presence_ctx.publish(self.redis_presence_channel, publish_data)
                    to_profile_model = Profile.get((Profile.id == request_data["to_profileid"]))
                    from_profile_model = Profile.get((Profile.id == request_data["from_profileid"]))
                    Buddy.insert(to_profile=to_profile_model,from_profile=from_profile_model).execute()
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

    #Get the buddies for a profile.
    def handle_buddies_search(self, request_data):
        if "profileid" not in request_data:
            return False

        profile = Profile.get(Profile.id == request_data["profileid"])

        buddies = Buddy.select().where((Buddy.from_profile == profile))
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
        print("Reverse lookup got: {}\n".format(request_data))

        try:
            profile = Profile.get((Profile.id == request_data["profileid"]) & (Profile.deleted == False))
            buddies = Buddy.select().where((Buddy.to_profile << request_data["target_profileids"])).execute()
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
    def handle_get_buddy_revokes(self, request_data):
        response = []
        print("Get revokes: {}\n".format(request_data))
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
        request_body = env['wsgi.input'].read(request_body_size)
        jwt_decoded = jwt.decode(request_body, self.SECRET_PROFILEMGR_KEY, algorithm='HS256')

        response = {}

        success = False

        if "mode" not in jwt_decoded:
            response['error'] = "INVALID_MODE"
            return jwt.encode(response, self.SECRET_PROFILEMGR_KEY, algorithm='HS256')    

        if jwt_decoded["mode"] == "update_profile":
            success = self.handle_update_profile(jwt_decoded)
        elif jwt_decoded["mode"] == "get_profile":
            profile = self.handle_get_profile(jwt_decoded)
            success = profile != None
            response['profile'] = profile            
        elif jwt_decoded["mode"] == "get_profiles":
            profiles = self.handle_get_profiles(jwt_decoded)
            success = True
            response['profiles'] = profiles
        elif jwt_decoded["mode"] == "create_profile":
            profile = self.handle_create_profile(jwt_decoded)
            if profile != None:
                success = True
                response['profile'] = profile
        elif jwt_decoded["mode"] == "delete_profile":
            success = self.handle_delete_profile(jwt_decoded)
        elif jwt_decoded["mode"] == "profile_search":
            profiles = self.handle_profile_search(jwt_decoded)
            response['profiles'] = profiles
            success = True
        elif jwt_decoded["mode"] == "authorize_buddy_add":
            success = self.handle_authorize_buddy_add(jwt_decoded)
        elif jwt_decoded["mode"] == "buddies_search":
            profiles = self.handle_buddies_search(jwt_decoded, False)
            response['profiles'] = profiles
            success = True
        elif jwt_decoded["mode"] == "buddies_reverse_search": #get who has who on given profileid
            profiles = self.handle_reverse_buddies_search(jwt_decoded)
            response['profiles'] = profiles
            success = True
        elif jwt_decoded["mode"] == "del_buddy":
            success = self.handle_del_buddy(jwt_decoded)
        elif jwt_decoded["mode"] == "get_buddies_revokes":
            revokes = self.handle_get_buddy_revokes(jwt_decoded)
            response['revokes'] = revokes
            success = True

        response['success'] = success
        start_response('200 OK', [('Content-Type','text/html')])
        return jwt.encode(response, self.SECRET_PROFILEMGR_KEY, algorithm='HS256')