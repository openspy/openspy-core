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

class OS_RegisterSvc(BaseService):
    # expects following vars:
    #   partnercode - 0
    #   email - if only email provided its a userid login
    #   email uniquenick namespaceid - profile login
    #   email nick namespaceid - profile login
    def try_login(self, register_options):

    	#perform user registration
        passthrough_user_params = ["password", "email", "partnercode"]
        user_data = {}
        for param in passthrough_user_params:
        	if param in register_options:
        		user_data[param] = register_options[param]

        params = jwt.encode(user_data, self.SECRET_REGISTER_KEY, algorithm='HS256')
        #params = urllib.urlencode(params)
        
        headers = {"Content-type": "application/x-www-form-urlencoded","Accept": "text/plain"}
        conn = httplib.HTTPConnection(self.REGISTER_SERVER)

        conn.request("POST", self.REGISTER_SCRIPT, params, headers)
        response = conn.getresponse().read()
        response = jwt.decode(response, self.SECRET_REGISTER_KEY, algorithm='HS256')

        print("Create user resp: {}\n".format(response))

        #perform profile creation
        if response["success"]:
        	user = response['user']
	        passthrough_profile_params = ["nick", "uniquenick", "namespaceid"]

	        profile = {}
	        for key in passthrough_profile_params:
	            if key in register_options:
	                profile[key] = register_options[key]

	        request_data = {'userid': user['id'], 'mode': 'create_profile', 'profile': profile}

	        params = jwt.encode(request_data, self.SECRET_PROFILEMGR_KEY, algorithm='HS256')
	        
	        
	        headers = {"Content-type": "application/x-www-form-urlencoded","Accept": "text/plain"}

	        conn = httplib.HTTPConnection(self.PROFILE_MGR_SERVER)

	        conn.request("POST", self.PROFILE_MGR_SCRIPT, params, headers)
	        response = conn.getresponse().read()
	        response = jwt.decode(response, self.SECRET_PROFILEMGR_KEY, algorithm='HS256')
	        print("Create profile: {}\n".format(response))
	        response["user"] = user

	        #create auth session
	        login_data = {}

	        login_data["save_session"] = True
	        login_data["email"] = register_options["email"]
	        login_data["password"] = register_options["password"]	        
	        login_data["partnercode"] = register_options["partnercode"]


	        params = jwt.encode(login_data, self.SECRET_AUTH_KEY, algorithm='HS256')
	        #params = urllib.urlencode(params)
	        
	        headers = {"Content-type": "application/x-www-form-urlencoded","Accept": "text/plain"}
	        conn = httplib.HTTPConnection(self.LOGIN_SERVER)

	        conn.request("POST", self.LOGIN_SCRIPT, params, headers)
	        response = conn.getresponse().read()
	        response = jwt.decode(response, self.SECRET_AUTH_KEY, algorithm='HS256')

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
