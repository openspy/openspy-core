from cgi import parse_qs, escape

import jwt

import MySQLdb
import uuid

from playhouse.shortcuts import model_to_dict, dict_to_model
from BaseModel import BaseModel
from Model.User import User

from BaseService import BaseService
import json
import uuid

class UserAccountMgrService(BaseService):

    def handle_update_user(self, data):
        user_model = User.get((User.id == data['user']['id']))
        for key in data['user']:
            if key != "id":
                setattr(user_model, key, data['user'][key])

        user_model.save()
        return True

    def handle_get_user(self, data):
        user = None
        print("GetUser got: {}\n".format(data))
        try:
            if "userid" in data:
                user = User.get((User.id == data["userid"]))
            elif "email" in data:
                user = User.get((User.email == data["email"]) & (User.partnercode == data["partnercode"]))
        except User.DoesNotExist:
            return None

        user = model_to_dict(user)
        del user['password']
        return user
 
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
        jwt_decoded = jwt.decode(request_body, self.SECRET_USERMGR_KEY, algorithm='HS256')

        response = {}

        print("Run: {}\n".format(jwt_decoded))

        success = False
        if "mode" not in jwt_decoded:
            response['error'] = "INVALID_MODE"

        if jwt_decoded["mode"] == "update_user":
            success = self.handle_update_user(jwt_decoded)
        elif jwt_decoded['mode'] == 'get_user':
            user = self.handle_get_user(jwt_decoded)
            if user != None:
                success = True
                response['user'] = user
     
        response['success'] = success

        start_response('200 OK', [('Content-Type','text/html')])
        return jwt.encode(response, self.SECRET_USERMGR_KEY, algorithm='HS256')