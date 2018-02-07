from cgi import parse_qs, escape
import xml.etree.ElementTree as ET

from collections import OrderedDict

from BaseService import BaseService

import redis

import simplejson as json

import http.client

class OS_WebAuth(BaseService):
    def __init__(self):
        BaseService.__init__(self)
        self.LOGIN_RESPONSE_SUCCESS = 0
        self.LOGIN_RESPONSE_SERVERINITFAILED = 1
        self.LOGIN_RESPONSE_USER_NOT_FOUND = 2
        self.LOGIN_RESPONSE_INVALID_PASSWORD = 3
        self.LOGIN_RESPONSE_INVALID_PROFILE = 4
        self.LOGIN_RESPONSE_UNIQUE_NICK_EXPIRED = 5
        self.LOGIN_RESPONSE_DB_ERROR = 6
        self.LOGIN_RESPONSE_SERVER_ERROR = 7
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
        
        headers = {"Content-type": "application/json","Accept": "text/plain"}
        conn = http.client.HTTPConnection(self.LOGIN_SERVER)

        conn.request("POST", self.LOGIN_SCRIPT, params, headers)
        response = json.loads(conn.getresponse().read())

        if response["success"] == False:
            if "reason" in response:
                if response["reason"] == 3:
                    response["reason"] = "INVALID_PASS"
                elif response["reason"] == 2:
                    response["reason"] = "USER_NOT_FOUND"
                elif response["reason"] == 4:
                    response["reason"] = "INVALID_PROFILE"
        return response
    def test_session(self, login_options):
        if "userid" not in login_options:
            return False
        if "session_key" not in login_options:
            return False

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

        if "valid" in response:
            return response["valid"]

        return False
    def del_session(self, login_options):
        if "userid" not in login_options:
            return False
        if "session_key" not in login_options:
            return False

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

        if "valid" in response:
            return response["valid"]
        return False
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

        if "mode" in request_body:
            if request_body["mode"] == "test_session":
                resp = self.test_session(request_body)
            elif request_body["mode"] == "auth":
                resp = self.try_login(request_body)
            elif request_body["mode"] == "del_session":
                resp = self.del_session(request_body)
        else:
            resp = self.try_login(request_body)
        return resp
