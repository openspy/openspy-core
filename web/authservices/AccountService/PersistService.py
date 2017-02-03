from cgi import parse_qs, escape

import jwt

import base64

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
        return {'data': base64.b64encode("test")}

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
        request_body = env['wsgi.input'].read(request_body_size)
        jwt_decoded = jwt.decode(request_body, self.SECRET_PERSIST_KEY, algorithm='HS256')

        print("JWT: {}\n".format(jwt_decoded))

        start_response('200 OK', [('Content-Type','text/html')])

        if "type" in jwt_decoded:
            if jwt_decoded["type"] == "newgame":
                new_game_data = self.handle_new_game(jwt_decoded)
                response['success'] = True
                response["data"] = new_game_data
            elif jwt_decoded["type"] == "updategame":
                update_game_data = self.handle_update_game(jwt_decoded)
                response['success'] = True
                response["data"] = update_game_data
            elif jwt_decoded["type"] == "set_persist_data":
                update_game_data = self.handle_set_data(jwt_decoded)
                response['success'] = True
                response["data"] = update_game_data
            elif jwt_decoded["type"] == "get_persist_data":
                update_game_data = self.handle_get_data(jwt_decoded)
                response['success'] = True
                response["data"] = update_game_data


        return jwt.encode(response, self.SECRET_PERSIST_KEY, algorithm='HS256')
