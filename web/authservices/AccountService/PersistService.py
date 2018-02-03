from cgi import parse_qs, escape

import base64
import json

from BaseService import BaseService

from lib.Exceptions.OS_BaseException import OS_BaseException
from lib.Exceptions.OS_CommonExceptions import *

class PersistService(BaseService):
    def __init__(self):
        BaseService.__init__(self)

    def handle_new_game(self, request_body):
        response = {}
        data = {'game_identifier': "NOT_IMPLEMENTED"}
        response['success'] = True
        response["data"] = data
        return response
    def handle_update_game(self, request_body):
        response = {}
        response['success'] = True
        response["data"] = {}
        return response
    def handle_set_data(self, request_body):
        if "data" in request_body:
            data = base64.b64decode(request_body["data"])
        return {}
    def handle_get_data(self, request_body):
        response = {'data': base64.b64encode("".encode('utf-8')), "success": True}
        return response

    def run(self, env, start_response):
        # the environment variable CONTENT_LENGTH may be empty or missing
        try:
            request_body_size = int(env.get('CONTENT_LENGTH', 0))
        except ValueError:
            request_body_size = 0


        response = {}
        response['success'] = False

        # When the method is POST the variable will be sent
        # in the HTTP request body which is passed by the WSGI server
        # in the file like wsgi.input environment variable.
        request_body = json.loads(env['wsgi.input'].read(request_body_size).decode('utf-8'))

        start_response('200 OK', [('Content-Type','application/json')])

        type_table = {
            "newgame": self.handle_new_game,
            "updategame": self.handle_update_game,
            "set_persist_data": self.handle_set_data,
            "get_persist_data": self.handle_get_data
        }
        try:
            if 'type' in request_body:
                req_type = request_body["mode"]
                if req_type in type_table:
                    response = type_table[req_type](request_body)
                else:
                    raise OS_InvalidMode()
        except OS_BaseException as e:
            response = e.to_dict()
        except Exception as error:
            response = {"error": repr(error)}

        return response
