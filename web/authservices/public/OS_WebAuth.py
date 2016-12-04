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

class OS_WebAuth(BaseService):
    # expects following vars:
    #   partnercode - 0
    #   email - if only email provided its a userid login
    #   email uniquenick namespaceid - profile login
    #   email nick namespaceid - profile login
    def try_login(self, login_options):
        print("Try: {}".format(login_options))

        login_data = {}
        passthrough_params = ["email","partnercode","uniquenick","namespaceid", "nick", "password","mode", "session_key", "userid"]
        for n in passthrough_params:
            if n in login_options:
                print("Login options: {}\n".format(n))
                login_data[n] = login_options[n]

        login_data['save_session'] = True #force a session key to be returned

        params = jwt.encode(login_data, self.SECRET_AUTH_KEY, algorithm='HS256')
        #params = urllib.urlencode(params)
        
        headers = {"Content-type": "application/x-www-form-urlencoded","Accept": "text/plain"}
        conn = httplib.HTTPConnection(self.LOGIN_SERVER)

        conn.request("POST", self.LOGIN_SCRIPT, params, headers)
        response = conn.getresponse().read()
        response = jwt.decode(response, self.SECRET_AUTH_KEY, algorithm='HS256')
        print("Got: {}\n".format(response))
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


        return json.dumps(self.try_login(request_body))
