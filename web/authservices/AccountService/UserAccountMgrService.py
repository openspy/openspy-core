from cgi import parse_qs, escape

import uuid

from playhouse.shortcuts import model_to_dict, dict_to_model
from BaseModel import BaseModel
from Model.User import User

from BaseService import BaseService
import json
import uuid

class UserAccountMgrService(BaseService):

    def handle_update_user(self, data):
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
        return True

    def handle_get_user(self, data):
        user = None
        user_data = data["user"]
        try:
            if "userid" in user_data:
                user = User.get((User.id == user_data["userid"]))
            elif "email" in user_data:
                user = User.get((User.email == user_data["email"]) & (User.partnercode == user_data["partnercode"]))
            if user:
                user = model_to_dict(user)
                del user['password']
        except User.DoesNotExist:
            return None

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
        input_data = env['wsgi.input'].read(request_body_size).decode('utf8')
        request_body = json.loads(input_data)

        response = {}

        print("Run: {}\n".format(request_body))

        success = False
        if "mode" not in request_body:
            response['error'] = "INVALID_MODE"

        if request_body["mode"] == "update_user":
            success = self.handle_update_user(request_body)
        elif request_body['mode'] == 'get_user':
            user = self.handle_get_user(request_body)
            if user != None:
                success = True
                response['user'] = user

        response['success'] = success

        start_response('200 OK', [('Content-Type','application/json')])
        return response
