from cgi import parse_qs, escape

import jwt

import uuid

from playhouse.shortcuts import model_to_dict, dict_to_model

import BaseService
from Model.User import User
from Model.Profile import Profile

from BaseService import BaseService

class AuthService(BaseService):

    def __init__(self):
        self.SECRET_AUTH_KEY = "dGhpc2lzdGhla2V5dGhpc2lzdGhla2V5dGhpc2lzdGhla2V5"

        self.LOGIN_RESPONSE_SUCCESS = 0
        self.LOGIN_RESPONSE_SERVERINITFAILED = 1
        self.LOGIN_RESPONSE_USER_NOT_FOUND = 2
        self.LOGIN_RESPONSE_INVALID_PASSWORD = 3
        self.LOGIN_RESPONSE_INVALID_PROFILE = 4
        self.LOGIN_RESPONSE_UNIQUE_NICK_EXPIRED = 5
        self.LOGIN_RESPONSE_DB_ERROR = 6
        self.LOGIN_RESPONSE_SERVER_ERROR = 7


    def get_profile_by_uniquenick(self, uniquenick, namespaceid, partnercode):
        if namespaceid == 0:
            the_uniquenick = Profile.select().join(User).where((Profile.uniquenick == uniquenick) & (User.partnercode == partnercode)).get()
        else:
            the_uniquenick = Profile.select().join(User).where((Profile.uniquenick == uniquenick) & (Profile.namespaceid == namespaceid) & (User.partnercode == partnercode)).get()
        return the_uniquenick

    def get_profile_by_nick_email(self, nick, email, partnercode):
        return Profile.select().join(User).where((Profile.nick == nick) & (User.partnercode == partnercode) & (User.email == email)).get()
    def test_pass_plain_by_userid(self, userid, password):
        auth_success = False

        try:
            matched_user = User.select().where((User.id == userid) & (User.password == password)).get()
            if matched_user:
                auth_success = True
        except User.DoesNotExist:
            return False
        return auth_success

    def create_auth_session(self, profile):
        session_key = uuid.uuid1()
        redis_key = '{}:{}'.format(profile['userid'],session_key)
        return {'redis_key': redis_key, 'session_key': session_key}

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
        jwt_decoded = jwt.decode(request_body, self.SECRET_AUTH_KEY, algorithm='HS256')

        if 'namespaceid' not in jwt_decoded:
            jwt_decoded['namespaceid'] = 0
        if 'partnercode' not in jwt_decoded:
            jwt_decoded['partnercode'] = 0

        hash_type = 'plain'
        if 'hash_type' in jwt_decoded:
            hash_type = jwt_decoded['hash_type']

        if 'password' not in jwt_decoded:
            response['reason'] = self.LOGIN_RESPONSE_INVALID_PASSWORD
            return jwt.encode(response, self.SECRET_AUTH_KEY, algorithm='HS256')

        profile = None
        if 'uniquenick' in jwt_decoded:
            profile = self.get_profile_by_uniquenick(jwt_decoded['uniquenick'], jwt_decoded['namespaceid'], jwt_decoded['partnercode'])
            if profile == None:
                response['reason'] = self.LOGIN_RESPONSE_INVALID_PROFILE
        elif 'profilenick' in jwt_decoded and 'email' in jwt_decoded:
            profile = self.get_profile_by_nick_email(jwt_decoded['profilenick'], jwt_decoded['email'], jwt_decoded['partnercode'])
            if profile == None:
                response['reason'] = self.LOGIN_RESPONSE_INVALID_PROFILE
        else:
            response['reason'] = self.LOGIN_RESPONSE_SERVER_ERROR
            return jwt.encode(response, self.SECRET_AUTH_KEY, algorithm='HS256')

        auth_success = False
        if hash_type == 'plain':
            auth_success = self.test_pass_plain_by_userid(profile.user.id, jwt_decoded['password'])

        if not auth_success:
            response['reason'] = self.LOGIN_RESPONSE_INVALID_PASSWORD
        else:
            response['success'] = True
            response['profile'] = model_to_dict(profile)
            response['expiretime'] = 10000 #TODO: figure out what this is used for, should make unix timestamp of expire time
            del response['profile']['user']['password']


        return jwt.encode(response, self.SECRET_AUTH_KEY, algorithm='HS256')
