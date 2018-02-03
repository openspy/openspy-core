from cgi import parse_qs, escape

import uuid

from playhouse.shortcuts import model_to_dict, dict_to_model
from BaseModel import BaseModel
from Model.User import User

from BaseService import BaseService
import json
import uuid

from lib.Exceptions.OS_BaseException import OS_BaseException
from lib.Exceptions.OS_CommonExceptions import *

class UserAccountMgrService(BaseService):

    def handle_update_user(self, data):
        response = {"success": False}
        if "user" not in data:
            data = {'user': data}
        user_model = User.get((User.id == data['user']['id']))

        if "email" in data["user"]:
            if data["user"]["email"] != user_model.email:
                user_model.email_verified = False

        if "email_verified" in data["user"]:
            del data["user"]["email_verified"]

        for key in data['user']:
            if key != "id":
                setattr(user_model, key, data['user'][key])

        user_model.save()
        response["success"] = True
        return response

    def handle_get_user(self, data):
        user = None
        if "user" not in data:
            raise OS_MissingParam("user")
        user_data = data["user"]
        response = {"success": False}
        try:
            if "userid" in user_data:
                user = User.get((User.id == user_data["userid"]))
            elif "email" in user_data:
                user = User.get((User.email == user_data["email"]) & (User.partnercode == user_data["partnercode"]))
            if user:
                user = model_to_dict(user)
                del user['password']
                response["user"] = user
                response["success"] = True
        except User.DoesNotExist:
            return None

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
        input_data = env['wsgi.input'].read(request_body_size).decode('utf8')
        request_body = json.loads(input_data)

        response = {}

        mode_table = {
            "update_user": self.handle_update_user,
            "get_user": self.handle_get_user
        }

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