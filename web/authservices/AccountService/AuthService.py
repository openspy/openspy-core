from cgi import parse_qs, escape

import jwt

import uuid

from playhouse.shortcuts import model_to_dict, dict_to_model

import BaseService
from Model.User import User
from Model.Profile import Profile

from BaseService import BaseService

import redis
from sha import sha

class AuthService(BaseService):

    def __init__(self):
        BaseService.__init__(self)
        self.redis_ctx = redis.StrictRedis(host='localhost', port=6379, db = 3)
        self.LOGIN_RESPONSE_SUCCESS = 0
        self.LOGIN_RESPONSE_SERVERINITFAILED = 1
        self.LOGIN_RESPONSE_USER_NOT_FOUND = 2
        self.LOGIN_RESPONSE_INVALID_PASSWORD = 3
        self.LOGIN_RESPONSE_INVALID_PROFILE = 4
        self.LOGIN_RESPONSE_UNIQUE_NICK_EXPIRED = 5
        self.LOGIN_RESPONSE_DB_ERROR = 6
        self.LOGIN_RESPONSE_SERVER_ERROR = 7


    def get_profile_by_uniquenick(self, uniquenick, namespaceid, partnercode):
        try:
            if namespaceid == 0:
                the_uniquenick = Profile.select().join(User).where((Profile.uniquenick == uniquenick) & (User.partnercode == partnercode)).get()
            else:
                the_uniquenick = Profile.select().join(User).where((Profile.uniquenick == uniquenick) & (Profile.namespaceid == namespaceid) & (User.partnercode == partnercode)).get()
            return the_uniquenick
        except User.DoesNotExist:
            return False

    def get_profile_by_nick_email(self, nick, email, partnercode):
        return Profile.select().join(User).where((Profile.nick == nick) & (User.partnercode == partnercode) & (User.email == email)).get()
    def test_pass_plain_by_userid(self, userid, password):
        auth_success = False
        try:
            matched_user = User.select().where((User.id == userid) & (User.password == password)).get()
            if matched_user:
                auth_success = True
        except User.DoesNotExist:
            auth_success = False
        return auth_success

    def create_auth_session(self, profile, user):
        session_key = sha(str(uuid.uuid1()))
        session_key.update(str(uuid.uuid4()))

        session_key = session_key.hexdigest()
        redis_key = '{}:{}'.format(user.id,session_key)

        self.redis_ctx.hset(redis_key, 'userid', user.id)
        if profile:
            self.redis_ctx.hset(redis_key, 'profileid', profile.id)

        self.redis_ctx.hset(redis_key, 'auth_token', session_key)
        return {'redis_key': redis_key, 'session_key': session_key}
    def set_auth_context(self, profile):
        if profile == False:
            self.redis_ctx.hdel(redis_key, 'profileid')
        else:
            self.redis_ctx.hset(redis_key, 'profileid', profile.id)

    def get_user_by_email(self, email, partnercode):
        try:
            return User.select().where((User.email == email) & (User.partnercode == partnercode)).get()
        except User.DoesNotExist:
            return False

    def test_session(self, params):
        if "userid" not in params or "session_key" not in params:
            return {"valid": False}
        if self.redis_ctx.exists("{}:{}".format(int(params["userid"]), params["session_key"])):
            return {"valid": True}
        else:
            return {"valid": False}

    def test_session_by_profileid(self, params):
        if "profileid" not in params or "session_key" not in params:
            return {'valid': False}
        print("Profile Test: {}\n".format(params))
        try:
            profile = Profile.get((Profile.id == params["profileid"]))
            test_params = {'session_key': params["session_key"], 'userid': profile.userid}
            return self.test_session(test_params)
        except Profile.DoesNotExist:
            return {'valid': False}
    def run(self, env, start_response):
        # the environment variable CONTENT_LENGTH may be empty or missing
        try:
            request_body_size = int(env.get('CONTENT_LENGTH', 0))
        except (ValueError):
            request_body_size = 0

        response = {}
        response['success'] = False

        start_response('200 OK', [('Content-Type','text/html')])

        # When the method is POST the variable will be sent
        # in the HTTP request body which is passed by the WSGI server
        # in the file like wsgi.input environment variable.
        request_body = env['wsgi.input'].read(request_body_size)
        jwt_decoded = jwt.decode(request_body, self.SECRET_AUTH_KEY, algorithm='HS256')

        if 'mode' in jwt_decoded:
            if jwt_decoded['mode'] == 'test_session':
                response = self.test_session(jwt_decoded)
                return jwt.encode(response, self.SECRET_AUTH_KEY, algorithm='HS256')
            elif jwt_decoded['mode'] == 'test_session_profileid':
                response = self.test_session_by_profileid(jwt_decoded)
                return jwt.encode(response, self.SECRET_AUTH_KEY, algorithm='HS256')

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
        elif "email" in jwt_decoded:
            response['reason'] = self.LOGIN_RESPONSE_SERVER_ERROR
            user = self.get_user_by_email(jwt_decoded['email'], jwt_decoded['partnercode'])

        auth_success = False
        if hash_type == 'plain' and profile != None:
            auth_success = self.test_pass_plain_by_userid(profile.user.id, jwt_decoded['password'])
        elif hash_type == 'plain' and user != None:
            auth_success = self.test_pass_plain_by_userid(user.id, jwt_decoded['password'])

        if not auth_success:
            response['reason'] = self.LOGIN_RESPONSE_INVALID_PASSWORD
        else:
            response['success'] = True
            if profile != None:
                response['profile'] = model_to_dict(profile)
                del response['profile']['user']['password']
            elif user != None:
                response['user'] = model_to_dict(user)
                del response['user']['password']

            response['expiretime'] = 10000 #TODO: figure out what this is used for, should make unix timestamp of expire time
            
        if "save_session" in jwt_decoded and jwt_decoded["save_session"] == True:

            if profile != None:
                session_data = self.create_auth_session(profile, profile.user)
            elif user != None:
                session_data = self.create_auth_session(False, user)
            response['session_key'] = session_data['session_key']

        return jwt.encode(response, self.SECRET_AUTH_KEY, algorithm='HS256')
