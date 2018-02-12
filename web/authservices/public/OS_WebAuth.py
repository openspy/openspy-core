from cgi import parse_qs, escape
import xml.etree.ElementTree as ET

from collections import OrderedDict

from BaseService import BaseService

import redis

import simplejson as json

import http.client

from lib.Exceptions.OS_BaseException import OS_BaseException
from lib.Exceptions.OS_CommonExceptions import *

class OS_WebAuth(BaseService):
    def __init__(self):
        BaseService.__init__(self)

    # expects following vars:
    #   partnercode - 0
    #   email - if only email provided its a userid login
    #   email uniquenick namespaceid - profile login
    #   email nick namespaceid - profile login
    def try_login(self, login_options):
        login_data = {}

        user_params = ["email","partnercode", "password", "userid"]
        profile_params = ["uniquenick","namespaceid", "nick"]
        passthrough_params = ["mode", "session_key"]
        user_obj = {}
        profile_obj = {}
        for n in user_params:
            if n in login_options:
                user_obj[n] = login_options[n]

        for n in profile_params:
            if n in login_options:
                profile_params[n] = login_options[n]

        for n in passthrough_params:
            if n in login_options:
                login_data[n] = login_options[n]

        login_data["user"] = user_obj
        login_data["profile"] = profile_obj
        login_data['save_session'] = True #force a session key to be returned
        login_data["mode"] = "auth"

        params = json.dumps(login_data)

        print("Dumped params: {}\n".format(params))
        
        headers = { "Content-type": "application/json",
                    "Accept": "text/plain", 
                    "APIKey": self.BACKEND_PRIVATE_APIKEY
        }
        conn = http.client.HTTPConnection(self.LOGIN_SERVER)

        conn.request("POST", self.LOGIN_SCRIPT, params, headers)
        response = conn.getresponse().read()

        response = json.loads(response)

        return response
    def test_session(self, login_options):
        response = {"success": False}
        if "userid" not in login_options:
            raise OS_MissingParam("userid")
        if "session_key" not in login_options:
            raise OS_MissingParam("session_key")

        send_data = {}
        send_data["userid"] = login_options["userid"]
        send_data["session_key"] = login_options["session_key"]

        params = json.dumps(send_data)
        
        headers = {
            "Content-type": "application/json",
            "Accept": "text/plain",
            "APIKey": self.BACKEND_PRIVATE_APIKEY
        }
        conn = http.client.HTTPConnection(self.LOGIN_SERVER)

        conn.request("POST", self.LOGIN_SCRIPT, params, headers)
        response = json.loads(conn.getresponse().read())

        if "valid" in response and response["valid"]:
            response["success"] = True

        return response
    def del_session(self, login_options):
        response = {"success": False}
        if "userid" not in login_options:
            raise OS_MissingParam("userid")
        if "session_key" not in login_options:
            raise OS_MissingParam("session_key")

        send_data = {}
        send_data["userid"] = login_options["userid"]
        send_data["session_key"] = login_options["session_key"]
        send_data["mode"] = "del_session"

        params = json.dumps(send_data)
        
        headers = {
            "Content-type": "application/json",
            "Accept": "text/plain",
            "APIKey": self.BACKEND_PRIVATE_APIKEY
        }

        conn = http.client.HTTPConnection(self.LOGIN_SERVER)

        conn.request("POST", self.LOGIN_SCRIPT, params, headers)
        response = json.loads(conn.getresponse().read())

        if "valid" in response and response["valid"]:
            response["success"] = True
        return response
    def run(self, env, start_response):
        # the environment variable CONTENT_LENGTH may be empty or missing
        try:
            request_body_size = int(env.get('CONTENT_LENGTH', 0))
        except (ValueError):
            request_body_size = 0

        # When the method is POST the variable will be sent
        # in the HTTP request body which is passed by the WSGI server
        # in the file like wsgi.input environment variable.
        request_body = json.loads(env['wsgi.input'].read(request_body_size))
       # d = parse_qs(request_body)

        start_response('200 OK', [('Content-Type','application/json')])

        if "mode" not in request_body:
            request_body["mode"] = "login"

        mode_table = {
            "test_session": self.test_session,
            "auth": self.try_login,
            "del_session":  self.del_session,
            "login": self.try_login
        }

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

        print("Auth: {}\n{}\n".format(request_body, response))
        return response