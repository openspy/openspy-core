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
import redis

class RegistrationService(BaseService):

    REGISTRATION_ERROR_NO_PASS = 1
    REGISTRATION_ERROR_NO_PARTNERCODE = 2

    def __init__(self):
        BaseService.__init__(self)
        self.redis_ctx = redis.StrictRedis(host='localhost', port=6379, db = 4)

    def send_verification_email(self, user):
        if user['email_verified'] == True:
            return None
        verification_key = uuid.uuid1()

        redis_key = "emailveri_{}".format(user['id'])
        self.redis_ctx.set(redis_key, verification_key)
        return verification_key

    def try_register_user(self, register_data):
        try:
            existing_user = User.select().where((User.email == register_data['email']) & (User.partnercode == int(register_data['partnercode']))).get()        
        except User.DoesNotExist:
            user = User.create(email=register_data['email'], partnercode=register_data['partnercode'], password=register_data['password'])
            user = model_to_dict(user)
            self.send_verification_email(user)            
            del user['password']
            
            return user

        return None

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
        jwt_decoded = jwt.decode(request_body, self.SECRET_REGISTER_KEY, algorithm='HS256')

        if 'partnercode' not in jwt_decoded:
            response['reason'] = self.REGISTRATION_ERROR_NO_PARTNERCODE
            return jwt.encode(response, self.SECRET_REGISTER_KEY, algorithm='HS256')

        if 'password' not in jwt_decoded:
            response['reason'] = self.REGISTRATION_ERROR_NO_PASS
            return jwt.encode(response, self.SECRET_REGISTER_KEY, algorithm='HS256')

        user = self.try_register_user(jwt_decoded)
        if user != None:
            response['success'] = True
            response['user'] = user
     

        return jwt.encode(response, self.SECRET_REGISTER_KEY, algorithm='HS256')