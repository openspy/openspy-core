from cgi import parse_qs, escape

import uuid
import hashlib

from playhouse.shortcuts import model_to_dict, dict_to_model
from BaseModel import BaseModel
from Model.User import User
from Model.Profile import Profile

from BaseService import BaseService
import json
import uuid
import redis
import os

from lib.Exceptions.OS_BaseException import OS_BaseException
from lib.Exceptions.OS_CommonExceptions import *

class RegistrationService(BaseService):

    REGISTRATION_ERROR_NO_PASS = 1
    REGISTRATION_ERROR_NO_PARTNERCODE = 2
    REGISTRATION_EMAIL_PARTNERCODE_EXISTS = 3
    VERIFICATION_KEY_EXPIRE_SECS = 21600 #6 hours
    def __init__(self):
        BaseService.__init__(self)
        self.redis_ctx = redis.StrictRedis(host=os.environ['REDIS_SERV'], port=int(os.environ['REDIS_PORT']), db = 4)

    def send_verification_email(self, user):
        if user['email_verified'] == True:
            return None
        verification_key = hashlib.sha1(str(uuid.uuid1()).encode('utf-8'))
        verification_key.update(str(uuid.uuid4()).encode('utf-8'))
        verification_key = verification_key.hexdigest()


        redis_key = "emailveri_{}".format(user['id'])
        self.redis_ctx.set(redis_key, verification_key)
        self.redis_ctx.expire(redis_key, self.VERIFICATION_KEY_EXPIRE_SECS)

        email_body = """\
        <html>
            <body>
                Hello,

                You may validate your email by going to the following link:
                 http://accmgr.openspy.org/verify_email/{}/{}

                Thanks,
                    OpenSpy
            </body>
        </html>
        """.format(user['id'], verification_key)
        email_data = {'from': 'no-reply@openspy.org', 'to': user['email'],  'subject': 'Email Verification', 'body': email_body}
        self.sendEmail(email_data)
        return verification_key

    def try_register_user(self, request_data):
        required_params = ["email", "password", "partnercode"]
        response = {"success": False}
        for param in required_params:
            if param not in request_data:
                raise OS_MissingParam(param)
        try:
            existing_user = User.select().where((User.email == request_data['email']) & (User.partnercode == int(request_data['partnercode']))).get()
        except User.DoesNotExist:
            user = User.create(email=request_data['email'], partnercode=request_data['partnercode'], password=request_data['password'])
            user = model_to_dict(user)
            self.send_verification_email(user)
            del user['password']
            response["user"] = user
            response["success"] = True

        return response

    def handle_perform_verify_email(self, request_data):
        response = {}
        redis_key = "emailveri_{}".format(request_data['userid'])
        success = False
        user = None

        required_params = ["userid", "verifykey"]
        for param in required_params:
            if param not in request_data:
                raise OS_MissingParam(param)

        try:
            user = User.get((User.id == request_data['userid']))
            if self.redis_ctx.exists(redis_key):
                real_verification_key = self.redis_ctx.get(redis_key)
                if request_data['verifykey'] == real_verification_key:
                    success = True
                    user.email_verified = True
                    user.save()
                    self.redis_ctx.delete(redis_key)
        except User.DoesNotExist:
            raise OS_Auth_NoSuchUser()

        response['success'] = success
        return response

    def handle_resend_verify_email(self, request_data):
        response = {}
        success = False

        user_id = False
        if "id" in request_data:
            user_id = request_data["id"]
        elif "userid" in request_data:
            user_id = request_data["userid"]
        else:
            raise OS_MissingParam("userid")

        if user_id != False:
            redis_key = "emailveri_{}".format(user_id)

            if not self.redis_ctx.exists(redis_key):
                try:
                    user = User.get((User.id == user_id))
                    self.send_verification_email(model_to_dict(user))
                    success = True
                except User.DoesNotExist:
                    raise OS_Auth_NoSuchUser()
        response['success'] = success
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
        request_body = json.loads(env['wsgi.input'].read(request_body_size))

        mode_table = {
            "perform_verify_email": self.handle_perform_verify_email,
            "resend_verify_email": self.handle_resend_verify_email,
            "register": self.try_register_user,
        }

        if "mode" not in request_body:
            request_body["mode"] = "register"

        start_response('200 OK', [('Content-Type','application/json')])

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