from cgi import parse_qs, escape

import re
import uuid
import json

from playhouse.shortcuts import model_to_dict, dict_to_model

import BaseService
from Model.User import User
from Model.Profile import Profile

from BaseService import BaseService

import redis
import hashlib

import os

from AccountService.RegistrationService import RegistrationService
from AccountService.UserProfileMgrService import UserProfileMgrService
class AuthService(BaseService):
    def __init__(self):
        BaseService.__init__(self)
        self.redis_ctx = redis.StrictRedis(host=os.environ['REDIS_SERV'], port=int(os.environ['REDIS_PORT']), db = 3)
        self.PREAUTH_EXPIRE_TIME = 3600
        self.LOGIN_RESPONSE_SUCCESS = 0
        self.LOGIN_RESPONSE_SERVERINITFAILED = 1
        self.LOGIN_RESPONSE_USER_NOT_FOUND = 2
        self.LOGIN_RESPONSE_INVALID_PASSWORD = 3
        self.LOGIN_RESPONSE_INVALID_PROFILE = 4
        self.LOGIN_RESPONSE_UNIQUE_NICK_EXPIRED = 5
        self.LOGIN_RESPONSE_DB_ERROR = 6
        self.LOGIN_RESPONSE_SERVER_ERROR = 7
        self.CREATE_RESPONSE_UNIQUENICK_IN_USE = 8
        self.LOGIN_CREATE_RESPONSE_INVALID_NICK = 9
        self.LOGIN_CREATE_RESPONSE_INVALID_UNIQUENICK = 10
        self.LOGIN_RESPONSE_INVALID_EMAIL = 11

        self.PARTNERID_GAMESPY = 0
        self.PARTNERID_IGN = 10
        self.PARTNERID_EA = 20

        self.AUTH_EXPIRE_TIME = 86400

    def get_profile_by_uniquenick(self, uniquenick, namespaceid, partnercode):
        try:
            if namespaceid == 0:
                the_uniquenick = Profile.select().join(User).where((Profile.uniquenick == uniquenick) & (User.partnercode == partnercode) & (User.deleted == 0) & (Profile.deleted == 0)).get()
            else:
                the_uniquenick = Profile.select().join(User).where((Profile.uniquenick == uniquenick) & (Profile.namespaceid == namespaceid) & (User.partnercode == partnercode) & (User.deleted == 0) & (Profile.deleted == 0)).get()
            return the_uniquenick
        except Profile.DoesNotExist:
            return None

    def get_profile_by_nick_email(self, nick, namespaceid, email, partnercode):
        try:
            return Profile.select().join(User).where((Profile.nick == nick) & (Profile.namespaceid == namespaceid) & (User.email == email) & (User.deleted == 0) & (Profile.deleted == 0)).get()
        except Profile.DoesNotExist:
            return None

    def get_profile_by_id(self, profileid):
        try:
            return Profile.select().join(User).where((Profile.id == profileid) & (User.deleted == 0) & (Profile.deleted == 0)).get()
        except Profile.DoesNotExist:
            return None
    def test_pass_plain_by_userid(self, userid, password):
        auth_success = False
        try:
            matched_user = User.select().where((User.id == userid) & (User.password == password) & (User.deleted == 0)).get()
            if matched_user:
                auth_success = True
        except User.DoesNotExist:
            auth_success = False
        return auth_success
    def test_gp_uniquenick_by_profile(self, profile, client_challenge, server_challenge, client_response):
        md5_pw = hashlib.md5(str(profile.user.password).encode('utf-8')).hexdigest()
        if profile.user.partnercode != self.PARTNERID_GAMESPY:
            crypt_buf = "{}{}{}@{}{}{}{}".format(md5_pw, "                                                ",profile.user.partnercode,profile.uniquenick, client_challenge, server_challenge, md5_pw)
        else:
            crypt_buf = "{}{}{}{}{}{}".format(md5_pw, "                                                ",profile.uniquenick, client_challenge, server_challenge, md5_pw)

        true_resp = hashlib.md5(str(crypt_buf).encode('utf-8')).hexdigest()

        if profile.user.partnercode != self.PARTNERID_GAMESPY:
            proof = "{}{}{}@{}{}{}{}".format(md5_pw, "                                                ",profile.user.partnercode,profile.uniquenick, server_challenge, client_challenge, md5_pw)
        else:
            proof = "{}{}{}{}{}{}".format(md5_pw, "                                                ",profile.uniquenick, server_challenge, client_challenge, md5_pw)
        proof = hashlib.md5(str(proof).encode('utf-8')).hexdigest()
        if true_resp == client_response:
            return proof
        return None
    def test_gp_nick_email_by_profile(self, profile, client_challenge, server_challenge, client_response):
        md5_pw = hashlib.md5(str(profile.user.password).encode('utf-8')).hexdigest()
        if profile.user.partnercode != self.PARTNERID_GAMESPY:
            crypt_buf = "{}{}{}@{}@{}{}{}{}".format(md5_pw, "                                                ",profile.user.partnercode,profile.nick, profile.user.email, client_challenge, server_challenge, md5_pw)
        else:
            crypt_buf = "{}{}{}@{}{}{}{}".format(md5_pw, "                                                ",profile.nick, profile.user.email, client_challenge, server_challenge, md5_pw)

        true_resp = hashlib.md5(str(crypt_buf).encode('utf-8')).hexdigest()

        if profile.user.partnercode != self.PARTNERID_GAMESPY:
            proof = "{}{}{}@{}@{}{}{}{}".format(md5_pw, "                                                ",profile.user.partnercode,profile.nick, profile.user.email, server_challenge, client_challenge, md5_pw)
        else:
            proof = "{}{}{}@{}{}{}{}".format(md5_pw, "                                                ",profile.nick, profile.user.email, server_challenge, client_challenge, md5_pw)
        proof = hashlib.md5(str(proof).encode('utf-8')).hexdigest()

        if true_resp == client_response:
            return proof
        return None

    def gs_sesskey(self, sesskey):
        str = "%.8x" % (sesskey ^ 0x38f371e6)
        ret = ""
        i = 17
        for n in str:
            ret += chr(ord(n) + i)
            i = i+1
        return ret
    def test_gstats_sessionkey_response_by_profileid(self, profile, session_key, client_response):
        if profile == None:
            return False
        sess_key = self.gs_sesskey(session_key)
        pw_hashed = "{}{}".format(profile.user.password,sess_key).encode('utf-8')
        pw_hashed = hashlib.md5(pw_hashed).hexdigest()

        return pw_hashed == client_response
    def test_preauth_ticket(self, request_body):
        if "auth_token" not in request_body:
            return {'reason': self.LOGIN_RESPONSE_INVALID_PASSWORD}
        token = request_body['auth_token']

        response = {}

        auth_key = "auth_token_{}".format(token)
        if not self.redis_ctx.exists(auth_key) or "challenge" not in request_body:
            return {'reason': self.LOGIN_RESPONSE_INVALID_PASSWORD}
        profileid = int(self.redis_ctx.hget(auth_key, 'profileid'))
        profile = self.get_profile_by_id(profileid)
        real_challenge = self.redis_ctx.hget(auth_key, 'challenge').decode('utf-8')

        if real_challenge != request_body['challenge']:
            return {'reason': self.LOGIN_RESPONSE_INVALID_PASSWORD}

        response['profile'] = model_to_dict(profile)
        response["success"] = True
        response['expiretime'] = self.PREAUTH_EXPIRE_TIME
        return response
    def test_gp_preauth_by_ticket(self, request_body):
        
        response = {}

        token = request_body['auth_token']

        if not self.redis_ctx.exists("auth_token_{}".format(token)):
            return {'reason': self.LOGIN_RESPONSE_INVALID_PASSWORD}

        profileid = int(self.redis_ctx.hget("auth_token_{}".format(token), 'profileid'))
        profile = self.get_profile_by_id(profileid)
        challenge = self.redis_ctx.hget("auth_token_{}".format(token), 'challenge').decode('utf-8')
        challenge = str(challenge).encode('utf-8')

        md5_pw = hashlib.md5(challenge).hexdigest()
        crypt_buf = "{}{}{}{}{}{}".format(md5_pw, "                                                ",token, request_body['server_challenge'], request_body['client_challenge'], md5_pw)
        true_resp = hashlib.md5(str(crypt_buf).encode('utf-8')).hexdigest()
        response['profile'] = model_to_dict(profile)
        response["server_response"] = true_resp
        response["success"] = True

        self.redis_ctx.delete("auth_token_{}".format(token))
        return response
    def test_nick_email_by_profile(self, profile, password):
        return profile.user.password == password
    def create_auth_session(self, profile, user):
        session_key = hashlib.sha1(str(uuid.uuid1()).encode('utf-8'))
        session_key.update(str(uuid.uuid4()).encode('utf-8'))

        session_key = session_key.hexdigest()
        redis_key = '{}:{}'.format(user.id,session_key)

        self.redis_ctx.hset(redis_key, 'userid', user.id)
        if profile:
            self.redis_ctx.hset(redis_key, 'profileid', profile.id)

        self.redis_ctx.hset(redis_key, 'auth_token', session_key)
        self.redis_ctx.expire(redis_key, self.AUTH_EXPIRE_TIME)
        return {'redis_key': redis_key, 'session_key': session_key}
    def set_auth_context(self, key, profile):
        if profile == None:
            self.redis_ctx.hdel(key, 'profileid')
        else:
            self.redis_ctx.hset(key, 'profileid', profile.id)

    def get_user_by_email(self, email, partnercode):
        try:
            return User.select().where((User.email == email) & (User.partnercode == partnercode) & (User.deleted == 0)).get()
        except User.DoesNotExist:
            return None

    def get_user_by_userid(self, userid):
        try:
            return User.select().where((User.id == userid) & (User.deleted == 0)).get()
        except User.DoesNotExist:
            return None

    def test_session(self, params):
        if "userid" not in params or "session_key" not in params:
            return {"valid": False, "admin": False}
        cursor = 0
        servers = []
        key = None
        
        while True:
            msg_scan_key = "*:{}".format(params["session_key"])
            resp = self.redis_ctx.scan(cursor, msg_scan_key)
            cursor = resp[0]
            for item in resp[1]:
                key = item.decode('utf8')
                break
            if cursor == 0:
                break
        if key == None:
            return {"valid": False, "admin": False}

        session_user_id = self.redis_ctx.hget(key, "userid")
        if session_user_id:
            session_user_id = session_user_id.decode('utf8')

        session_user = self.get_user_by_userid(int(session_user_id))
        if session_user:
            is_admin = session_user.is_admin()
            if session_user.id == session_user_id or is_admin:
                return {"valid": True, "admin": is_admin, 'session_user': model_to_dict(session_user)}
        return {"valid": False, "admin": False}

    def test_session_by_profileid(self, params):
        if "profileid" not in params or "session_key" not in params:
            return {'valid': False}
        try:
            profile = Profile.get((Profile.id == params["profileid"]))
            test_params = {'session_key': params["session_key"], 'userid': profile.userid}
            return self.test_session(test_params)
        except Profile.DoesNotExist:
            return {'valid': False}
    def handle_make_auth_ticket(self, params):
        challenge = hashlib.sha1(str(uuid.uuid1()).encode('utf-8'))
        challenge.update(str(uuid.uuid4()).encode('utf-8'))
        challenge = challenge.hexdigest()

        token = hashlib.sha1(str(uuid.uuid1()).encode('utf-8'))
        token.update(str(uuid.uuid4()).encode('utf-8'))
        token = token.hexdigest()

        self.redis_ctx.hset("auth_token_{}".format(token), 'profileid', params['profileid'])
        self.redis_ctx.hset("auth_token_{}".format(token), 'challenge', challenge)
        self.redis_ctx.expire("auth_token_{}".format(token), self.PREAUTH_EXPIRE_TIME)


        return {"ticket": token, "challenge": challenge, "success": True}
    def handle_delete_session(self, params):
        userid = None
        session_key = None
        if "profileid" in params:
            profile = Profile.get((Profile.id == params["profileid"]))
            userid = profile.userid
        elif "userid" in params:
            userid = params["userid"]

        if "session_key" in params:
            session_key = params["session_key"]
        if userid != None:
            redis_key = "{}:{}".format(int(userid), session_key)
            if self.redis_ctx.exists(redis_key):
                self.redis_ctx.delete(redis_key)
                return {'deleted': True}
        return {'deleted': False}
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

    def auth_or_create_profile(self, request_body):
        #{u'profilenick': u'sctest01', u'save_session': True, u'set_context': u'profile', u'hash_type': u'auth_or_create_profile', u'namespaceid': 1,
        #u'uniquenick': u'', u'partnercode': 0, u'password': u'gspy', u'email': u'sctest@gamespy.com'}
        user_where = (User.deleted == False)
        user_data = request_body["user"]
        if "email" in user_data:
            if re.match('^[_a-z0-9-]+(\.[_a-z0-9-]+)*@[a-z0-9-]+(\.[a-z0-9-]+)*(\.[a-z]{2,4})$', user_data["email"]) == None:
                return {'reason': self.LOGIN_RESPONSE_INVALID_EMAIL}
            user_where = (user_where) & (User.email == user_data["email"])
        if "partnercode" in user_data:
            user_data['partnercode'] = user_data["partnercode"]
            partnercode = user_data["partnercode"]
        else:
            partnercode = 0

        user_where = ((user_where) & (User.partnercode == partnercode))
        try:
            user = User.get(user_where)
            user = model_to_dict(user)
            if user['password'] != user_data["password"]:
                return {'reason': self.LOGIN_RESPONSE_INVALID_PASSWORD}
        except User.DoesNotExist:
            register_svc = RegistrationService()
            user = register_svc.try_register_user(user_data)


        profile_data = request_body["profile"] #create data
        profile_where = (Profile.deleted == False)
        force_unique = False
        if "uniquenick" in profile_data:
            force_unique = True
            profile_where = (profile_where) & (Profile.uniquenick == profile_data["uniquenick"])

        if "nick" in profile_data:
            profile_where = (profile_where) & (Profile.nick == profile_data['nick'])

        if "namespaceid" in profile_data:
            namespaceid = profile_data["namespaceid"]
        else:
            namespaceid = 0

        profile_where = (profile_where) & (Profile.namespaceid == namespaceid)

        ret = {}
        try:
            if not force_unique:
                profile_where = (profile_where) & (Profile.userid == user["id"])
            profile = Profile.get(profile_where)
            profile = model_to_dict(profile)
            del profile['user']['password']

            ret["new_profile"] = False

            if user["id"] != profile["user"]["id"]:
                return {'reason': self.LOGIN_CREATE_RESPONSE_INVALID_UNIQUENICK, "profile": profile}
        except Profile.DoesNotExist:
            user_profile_srv = UserProfileMgrService()
            profile = user_profile_srv.handle_create_profile({'profile': profile_data, 'user': user})
            ret["new_profile"] = True
            if "error" in profile:
                reason = 0
                if profile["error"] == "INVALID_UNIQUENICK":
                    reason = self.LOGIN_CREATE_RESPONSE_INVALID_UNIQUENICK
                elif profile["error"] == "INVALID_NICK":
                    reason = self.LOGIN_CREATE_RESPONSE_INVALID_NICK
                elif profile["error"] == "UNIQUENICK_IN_USE":
                    reason = self.CREATE_RESPONSE_UNIQUENICK_IN_USE
                ret = {'reason' : reason}
                if "profile" in profile:
                    ret["profile"] = profile["profile"]

                ret["new_profile"] = False
                return ret
        ret["profile"] = profile
        return ret
    def run(self, env, start_response):
        # the environment variable CONTENT_LENGTH may be empty or missing
        try:
            request_body_size = int(env.get('CONTENT_LENGTH', 0))
        except ValueError:
            request_body_size = 0

        response = {}
        response['success'] = False

        start_response('200 OK', [('Content-Type','application/json')])

        # When the method is POST the variable will be sent
        # in the HTTP request body which is passed by the WSGI server
        # in the file like wsgi.input environment variable.
        input_str = env['wsgi.input'].read(request_body_size).decode('utf-8')
        request_body = json.loads(input_str)

        if 'mode' in request_body:
            if request_body['mode'] == 'test_session':
                ret = self.test_session(request_body)
                return ret
            elif request_body['mode'] == 'test_session_profileid':
                return self.test_session_by_profileid(request_body)
            elif request_body['mode'] == "pwreset":
                return self.handle_pw_reset(request_body)
            elif request_body['mode'] == "perform_pwreset":
                return self.handle_perform_pw_reset(request_body)
            elif request_body['mode'] == "del_session":
                return self.handle_delete_session(request_body)
            elif request_body['mode'] == 'make_auth_ticket':
                return self.handle_make_auth_ticket(request_body)
            elif request_body["mode"] == 'test_preauth':
                return self.test_preauth_ticket(request_body)
        #mode == "auth"
        if 'namespaceid' not in request_body:
            request_body['namespaceid'] = 0
        if 'partnercode' not in request_body:
            request_body['partnercode'] = 0

        hash_type = 'plain'
        if 'hash_type' in request_body:
            hash_type = request_body['hash_type']

        profile = None
        user = None

        if "user" in request_body:
            user_body = request_body["user"]
        else:
            user_body = {}
        if "profile" in request_body:
            profile_body = request_body["profile"]
        else:
            profile_body = {}

        if 'password' not in user_body and hash_type == 'plain':
            response['reason'] = self.LOGIN_RESPONSE_INVALID_PASSWORD
            return response

        if 'uniquenick' in profile_body:
            profile = self.get_profile_by_uniquenick(profile_body['uniquenick'], profile_body['namespaceid'], user_body['partnercode'])
            if profile == None:
                response['reason'] = self.LOGIN_RESPONSE_INVALID_PROFILE
        elif 'nick' in profile_body and 'email' in user_body:
            profile = self.get_profile_by_nick_email(profile_body['nick'], profile_body['namespaceid'], user_body['email'], user_body['partnercode'])
            if profile == None:
                response['reason'] = self.LOGIN_RESPONSE_INVALID_PROFILE
        elif "email" in user_body:
            user = self.get_user_by_email(user_body['email'], user_body['partnercode'])
            if user == None:
                response['reason'] = self.LOGIN_RESPONSE_SERVER_ERROR
        elif "userid" in request_body:
            user = self.get_user_by_userid(request_body["userid"])
            if user == None:
                response['reason'] = self.LOGIN_RESPONSE_SERVER_ERROR
        elif "profileid" in request_body:
            profile = self.get_profile_by_id(request_body["profileid"])
        elif "id" in user_body:
            user = self.get_user_by_userid(user_body["id"])
            if user == None:
                response['reason'] = self.LOGIN_RESPONSE_SERVER_ERROR
        elif "id" in profile_body:
            profile = self.get_profile_by_id(profile_body["id"])
        auth_success = False


        if hash_type == 'plain' and profile != None:
            auth_success = self.test_pass_plain_by_userid(profile.user.id, user_body['password'])
        elif hash_type == 'plain' and user != None:
            auth_success = self.test_pass_plain_by_userid(user.id, user_body['password'])
            
        elif hash_type == 'gp_nick_email' and profile != None:
            proof = self.test_gp_nick_email_by_profile(profile, request_body['client_challenge'], request_body['server_challenge'], request_body['client_response'])
            if proof != None:
                auth_success = True
                response['server_response'] = proof
        elif hash_type == 'gp_uniquenick' and profile != None:
            proof = self.test_gp_uniquenick_by_profile(profile, request_body['client_challenge'], request_body['server_challenge'], request_body['client_response'])
            if proof != None:
                auth_success = True
                response['server_response'] = proof
        elif hash_type == 'nick_email' and profile != None:
            auth_success = self.test_nick_email_by_profile(profile, user_body['password'])
        elif hash_type == 'auth_or_create_profile':
            auth_resp = self.auth_or_create_profile(request_body)
            if "new_profile" in auth_resp:
                response['new_profile'] = auth_resp['new_profile']
            if "profile" in auth_resp:
                response["profile"] = auth_resp["profile"]
            if "reason" in auth_resp:
                response['reason'] = auth_resp['reason']
            elif isinstance(auth_resp, dict):
                response['success'] = True

                if 'reason' in response:
                    del response['reason']
                if "reason" in auth_resp:
                    response['success'] = False
                    response['reason'] = auth_resp['reason']

                auth_success = True
                #retrieve profile for save_session
                profile = Profile.get((Profile.id == response['profile']['id']))
                user = profile.user
        elif hash_type == "gstats_pid_sesskey":
            auth_success = self.test_gstats_sessionkey_response_by_profileid(profile, request_body["session_key"], request_body["client_response"])
        elif hash_type == "gp_preauth":
            response = self.test_gp_preauth_by_ticket(request_body)
            if "reason" not in response:
                auth_success = True
                profile = Profile.get((Profile.id == response['profile']['id']))
                user = profile.user
            else:
                response["success"] = False
        if not auth_success and "reason" not in response:
            if user == None or profile == None:
                response['reason'] = self.LOGIN_RESPONSE_USER_NOT_FOUND
            else:
                response['reason'] = self.LOGIN_RESPONSE_INVALID_PASSWORD
        elif "reason" not in response:
            response['success'] = True
            if profile != None:
                response['profile'] = model_to_dict(profile)
                del response['profile']['user']['password']
            elif user != None:
                response['user'] = model_to_dict(user)
                del response['user']['password']
            else:
                response['reason'] = self.LOGIN_RESPONSE_USER_NOT_FOUND

            response['expiretime'] = self.AUTH_EXPIRE_TIME #TODO: figure out what this is used for, should make unix timestamp of expire time

        if "save_session" in request_body and request_body["save_session"] == True and response['success'] == True:
            if profile != None:
                session_data = self.create_auth_session(profile, profile.user)
            elif user != None:
                session_data = self.create_auth_session(False, user)
            response['session_key'] = session_data['session_key']

        if "set_context" in request_body and "session_key" in response:
            if request_body["set_context"] == "profile":
                self.set_auth_context(response["session_key"], profile)

        return response
