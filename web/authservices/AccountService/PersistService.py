from cgi import parse_qs, escape

import base64
import json;

from BaseService import BaseService



class PersistService(BaseService):
    def __init__(self):
        BaseService.__init__(self)

    def handle_new_game(self, request):
        return {'game_identifier': "NOT_IMPLEMENTED"}
    def handle_update_game(self, request):
        return {}
    def handle_set_data(self, request):
        if "data" in request:
            data = base64.b64decode(request["data"])
            return True
        return False
    def handle_get_data(self, request):
        return {'data': base64.b64encode("".encode('utf-8'))}

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

        if "type" in request_body:
            if request_body["type"] == "newgame":
                new_game_data = self.handle_new_game(request_body)
                response['success'] = True
                response["data"] = new_game_data
            elif request_body["type"] == "updategame":
                update_game_data = self.handle_update_game(request_body)
                response['success'] = True
                response["data"] = update_game_data
            elif request_body["type"] == "set_persist_data":
                update_game_data = self.handle_set_data(request_body)
                response['success'] = True
                response["data"] = update_game_data
            elif request_body["type"] == "get_persist_data":
                update_game_data = self.handle_get_data(request_body)
                response['success'] = True
                response["data"] = update_game_data["data"]


        return response
