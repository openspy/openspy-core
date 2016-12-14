from cgi import parse_qs, escape

import jwt

import uuid

from playhouse.shortcuts import model_to_dict, dict_to_model

import BaseService
from Model.User import User
from Model.Profile import Profile

from BaseService import BaseService

import redis
from md5 import md5
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

        self.PARTNERID_GAMESPY = 0
        self.PARTNERID_IGN = 10


    def get_profile_by_uniquenick(self, uniquenick, namespaceid, partnercode):
        try:
            if namespaceid == 0:
                the_uniquenick = Profile.select().join(User).where((Profile.uniquenick == uniquenick) & (User.partnercode == partnercode) & (User.deleted == False) & (Profile.deleted == False)).get()
            else:
                the_uniquenick = Profile.select().join(User).where((Profile.uniquenick == uniquenick) & (Profile.namespaceid == namespaceid) & (User.partnercode == partnercode) & (User.deleted == False) & (Profile.deleted == False)).get()
            return the_uniquenick
        except Profile.DoesNotExist:
            return None

    def get_profile_by_nick_email(self, nick, email, partnercode):
        try:
            return Profile.select().join(User).where((Profile.nick == nick) & (User.partnercode == partnercode) & (User.email == email) & (User.deleted == False) & (Profile.deleted == False)).get()
        except Profile.DoesNotExist:
            return None
    def test_pass_plain_by_userid(self, userid, password):
        auth_success = False
        try:
            matched_user = User.select().where((User.id == userid) & (User.password == password) & (User.deleted == False)).get()
            if matched_user:
                auth_success = True
        except User.DoesNotExist:
            auth_success = False
        return auth_success

    def test_gp_nick_email_by_profile(self, profile, client_challenge, server_challenge, client_response):
        md5_pw = md5(profile.user.password).hexdigest()
        crypt_buf = "{}{}{}@{}{}{}{}".format(md5_pw, "                                                ",profile.nick, profile.user.email,client_challenge, server_challenge, md5_pw)
        true_resp = md5(crypt_buf).hexdigest()

        if profile.user.partnercode != self.PARTNERID_GAMESPY:
            proof = "{}{}{}@{}@{}{}{}{}".format(md5_pw, "                                                ",profile.user.partnercode,profile.nick, profile.user.email, server_challenge, client_challenge, md5_pw)
        else:
            proof = "{}{}{}@{}{}{}{}".format(md5_pw, "                                                ",profile.nick, profile.user.email, server_challenge, client_challenge, md5_pw)
        proof = md5(proof).hexdigest()
        if true_resp == client_response:
            return proof
        return None

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
    def set_auth_context(self, key, profile):
        if profile == None:
            self.redis_ctx.hdel(key, 'profileid')
        else:
            self.redis_ctx.hset(key, 'profileid', profile.id)

    def get_user_by_email(self, email, partnercode):
        try:
            return User.select().where((User.email == email) & (User.partnercode == partnercode) & (User.deleted == False)).get()
        except User.DoesNotExist:
            return None

    def get_user_by_userid(self, userid):
        try:
            return User.select().where((User.id == userid) & (User.deleted == False)).get()
        except User.DoesNotExist:
            return None

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
        try:
            profile = Profile.get((Profile.id == params["profileid"]))
            test_params = {'session_key': params["session_key"], 'userid': profile.userid}
            return self.test_session(test_params)
        except Profile.DoesNotExist:
            return {'valid': False}
    def do_password_reset(self, user):
        reset_key = sha(str(uuid.uuid1()))
        reset_key.update(str(uuid.uuid4()))
        reset_key = reset_key.hexdigest()

        redis_key = 'pwreset_{}'.format(user.id)
        self.redis_ctx.set(redis_key, reset_key)

        email_body = """\
        <html>
            <body>
                Hello, Your password reset request has been processed and your account can now be recovered at http://accmgr.openspy.org/forgot_password/{}/{}

                If you did not initiate this request please disregard this email,

                Thanks,
                    OpenSpy
            </body>
        </html>
        """.format(user.id, reset_key)
        email_data = {'from': 'no-reply@openspy.org', 'to': user.email,  'subject': 'Password Reset', 'body': email_body}
        self.sendEmail(email_data)
        return {'success': True}
    def handle_pw_reset(self, reset_data):
        try:
            user = User.select().where((User.email == reset_data["email"]) & (User.partnercode == reset_data["partnercode"])).get()
            self.do_password_reset(user)
        except User.DoesNotExist:
            return {'success': False}
        return {'success': True}
    def handle_perform_pw_reset(self, reset_data):
        required_fields = ["userid", "password", "resetkey"]
        try:
            user = User.select().where((User.id == reset_data["userid"])).get()
            user.password = reset_data["password"]
            redis_key = 'pwreset_{}'.format(user.id)
            real_reset_key = self.redis_ctx.get(redis_key)
            if real_reset_key == reset_data["resetkey"]:
                user.save()
                self.redis_ctx.delete(redis_key)
                return {'success': True}
            else:
                return {'success': False}
        except User.DoesNotExist:
            return {'success': False}
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
            elif jwt_decoded['mode'] == "pwreset":
                response = self.handle_pw_reset(jwt_decoded)
                return jwt.encode(response, self.SECRET_AUTH_KEY, algorithm='HS256')
            elif jwt_decoded['mode'] == "perform_pwreset":
                response = self.handle_perform_pw_reset(jwt_decoded)
                return jwt.encode(response, self.SECRET_AUTH_KEY, algorithm='HS256')

        #mode == "auth"
        if 'namespaceid' not in jwt_decoded:
            jwt_decoded['namespaceid'] = 0
        if 'partnercode' not in jwt_decoded:
            jwt_decoded['partnercode'] = 0

        hash_type = 'plain'
        if 'hash_type' in jwt_decoded:
            hash_type = jwt_decoded['hash_type']

        if 'password' not in jwt_decoded and hash_type == 'plain':
            response['reason'] = self.LOGIN_RESPONSE_INVALID_PASSWORD
            return jwt.encode(response, self.SECRET_AUTH_KEY, algorithm='HS256')
        profile = None
        user = None
        if 'uniquenick' in jwt_decoded:
            profile = self.get_profile_by_uniquenick(jwt_decoded['uniquenick'], jwt_decoded['namespaceid'], jwt_decoded['partnercode'])
            if profile == None:
                response['reason'] = self.LOGIN_RESPONSE_INVALID_PROFILE
        elif 'profilenick' in jwt_decoded and 'email' in jwt_decoded:
            profile = self.get_profile_by_nick_email(jwt_decoded['profilenick'], jwt_decoded['email'], jwt_decoded['partnercode'])
            if profile == None:
                response['reason'] = self.LOGIN_RESPONSE_INVALID_PROFILE
        elif "email" in jwt_decoded:
            user = self.get_user_by_email(jwt_decoded['email'], jwt_decoded['partnercode'])
            if user == None:
                response['reason'] = self.LOGIN_RESPONSE_SERVER_ERROR
        elif "userid" in jwt_decoded:
            user = self.get_user_by_userid(jwt_decoded["userid"])
            if user == None:
                response['reason'] = self.LOGIN_RESPONSE_SERVER_ERROR

        auth_success = False
        if hash_type == 'plain' and profile != None:
            auth_success = self.test_pass_plain_by_userid(profile.user.id, jwt_decoded['password'])
        elif hash_type == 'plain' and user != None:
            auth_success = self.test_pass_plain_by_userid(user.id, jwt_decoded['password'])
        elif hash_type == 'gp_nick_email' and profile != None:
            proof = self.test_gp_nick_email_by_profile(profile, jwt_decoded['client_challenge'], jwt_decoded['server_challenge'], jwt_decoded['client_response'])
            if proof != None:
                auth_success = True
                response['server_response'] = proof
        if not auth_success:
            if user == None or profile == None:
                response['reason'] = self.LOGIN_RESPONSE_USER_NOT_FOUND
            else:
                response['reason'] = self.LOGIN_RESPONSE_INVALID_PASSWORD
        else:
            response['success'] = True
            if profile != None:
                response['profile'] = model_to_dict(profile)
                del response['profile']['user']['password']
            elif user != None:
                response['user'] = model_to_dict(user)
                del response['user']['password']
            else:
                response['reason'] = self.LOGIN_RESPONSE_USER_NOT_FOUND

            response['expiretime'] = 10000 #TODO: figure out what this is used for, should make unix timestamp of expire time
            
        if "save_session" in jwt_decoded and jwt_decoded["save_session"] == True and response['success'] == True:
            if profile != None:
                session_data = self.create_auth_session(profile, profile.user)
            elif user != None:
                session_data = self.create_auth_session(False, user)
            response['session_key'] = session_data['session_key']

        if "set_context" in jwt_decoded and "session_key" in response:
            if jwt_decoded["set_context"] == "profile":
                self.set_auth_context(response["session_key"], profile)

        return jwt.encode(response, self.SECRET_AUTH_KEY, algorithm='HS256')
