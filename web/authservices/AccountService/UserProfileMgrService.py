from cgi import parse_qs, escape

import jwt

import MySQLdb
import uuid

from playhouse.shortcuts import model_to_dict, dict_to_model
from BaseModel import BaseModel
from Model.User import User
from Model.Profile import Profile

from BaseService import BaseService
import json
import uuid

class UserProfileMgrService(BaseService):

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
        print("Made PK: {}\n".format(profile_pk))
        profile = Profile.get((Profile.id == profile_pk))
        print("Profile: {}\n".format(profile))
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

        if jwt_decoded["mode"] == "update_profiles":
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
     
        response['success'] = success
        start_response('200 OK', [('Content-Type','text/html')])
        return jwt.encode(response, self.SECRET_PROFILEMGR_KEY, algorithm='HS256')