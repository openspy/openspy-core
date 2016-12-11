from cgi import parse_qs, escape
import xml.etree.ElementTree as ET

import binascii
from M2Crypto import RSA, BIO
import md5, struct, os

from collections import OrderedDict
import jwt

from BaseService import BaseService
import httplib, urllib, json

import redis

class OS_PWReset(BaseService):
    def process_initiate_pw_reset(self, request_data):
    	response = {}

    	reset_data = {}
    	passthrough_params = ["email", "partnercode"]
    	for key in request_data:
    		reset_data[key] = request_data[key]

    	reset_data["mode"] = "pwreset"
        params = jwt.encode(reset_data, self.SECRET_AUTH_KEY, algorithm='HS256')
        
        headers = {"Content-type": "application/x-www-form-urlencoded","Accept": "text/plain"}
        conn = httplib.HTTPConnection(self.LOGIN_SERVER)

        conn.request("POST", self.LOGIN_SCRIPT, params, headers)
        response = conn.getresponse().read()
        response = jwt.decode(response, self.SECRET_AUTH_KEY, algorithm='HS256')

        return response
    def process_perform_pw_reset(self, request_data):
        response = {}

        reset_data = {}
        passthrough_params = ["userid", "resetkey", "password"]
        for key in request_data:
            reset_data[key] = request_data[key]

        reset_data["mode"] = "perform_pwreset"
        params = jwt.encode(reset_data, self.SECRET_AUTH_KEY, algorithm='HS256')
        
        headers = {"Content-type": "application/x-www-form-urlencoded","Accept": "text/plain"}
        conn = httplib.HTTPConnection(self.LOGIN_SERVER)

        conn.request("POST", self.LOGIN_SCRIPT, params, headers)
        response = conn.getresponse().read()
        response = jwt.decode(response, self.SECRET_AUTH_KEY, algorithm='HS256')

        if response["success"] == True:
            #create auth session
            login_data = {}

            login_data["save_session"] = True
            login_data["userid"] = request_data["userid"]
            login_data["password"] = request_data["password"]


            params = jwt.encode(login_data, self.SECRET_AUTH_KEY, algorithm='HS256')
            
            headers = {"Content-type": "application/x-www-form-urlencoded","Accept": "text/plain"}
            conn = httplib.HTTPConnection(self.LOGIN_SERVER)

            conn.request("POST", self.LOGIN_SCRIPT, params, headers)
            response = conn.getresponse().read()
            response = jwt.decode(response, self.SECRET_AUTH_KEY, algorithm='HS256')
            return response


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

        start_response('200 OK', [('Content-Type','text/html')])

        response = {}

        if "mode" in request_body:
            if request_body["mode"] == "init":
                response = self.process_initiate_pw_reset(request_body)
            elif request_body["mode"] == "perform":
                response = self.process_perform_pw_reset(request_body)

        return json.dumps(response)
