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
            for profile in Profile.select().where((Profile.userid == data["userid"]) & (Profile.deleted == False)):
                profile_dict = model_to_dict(profile)
                del profile_dict['user']
                profiles.append(profile_dict)

        except Profile.DoesNotExist:
            return []
        return profiles

    def handle_create_profile(self, data):
        print("Create Profile: {}\n".format(data))
        profile_data = data["profile"]
        print("Profile: {}\n".format(profile_data))
        user = User.get((User.id == data["userid"]))
        profile_data["user"] = user
        profile_pk = Profile.insert(**profile_data).execute()
        profile = model_to_dict(Profile.get((Profile.id == profile_pk) & (Profile.deleted == False)))
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
        elif jwt_decoded["mode"] == "get_profiles":
            profiles = self.handle_get_profiles(jwt_decoded)
            success = True
            response['profiles'] = profiles
        elif jwt_decoded["mode"] == "create_profile":
            profile = self.handle_create_profile(jwt_decoded)
            success = True
            response['profile'] = profile
        elif jwt_decoded["mode"] == "delete_profile":
            success = self.handle_delete_profile(jwt_decoded)
     
        response['success'] = success
        return jwt.encode(response, self.SECRET_PROFILEMGR_KEY, algorithm='HS256')