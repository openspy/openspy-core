from cgi import parse_qs, escape
import xml.etree.ElementTree as ET

from collections import OrderedDict
import http.client

from BaseService import BaseService

import redis

import simplejson as json

from lib.Exceptions.OS_BaseException import OS_BaseException
from lib.Exceptions.OS_CommonExceptions import *

class OS_WebUserMgr(BaseService):
    def test_required_params(self, input, params, all_required):
        found_one = False
        for param in params:
            if param not in input:
                if all_required:
                    raise OS_MissingParam(param)
            else:
                found_one = True
        return all_required and found_one or not all_required

    def test_user_session(self, session_key, userid):
        send_data = {'session_key': session_key, 'userid': userid, 'mode': 'test_session'}
        params = json.dumps(send_data)
        
        headers = {
            "Content-type": "application/json",
            "Accept": "text/plain",
            "APIKey": self.BACKEND_PRIVATE_APIKEY
        }
        conn = http.client.HTTPConnection(self.LOGIN_SERVER)

        conn.request("POST", self.LOGIN_SCRIPT, params, headers)
        response = json.loads(conn.getresponse().read())

        return response
    def process_request(self, login_options):
        required_params = ['mode', 'user', 'session_key']
        required_user_params = ['email', 'id', 'userid']


        if not self.test_required_params(login_options, required_params, True):
            raise OS_MissingParam("required params")

        if not self.test_required_params(login_options['user'], required_user_params, False):
            raise OS_MissingParam("required user params")

        mode = login_options['mode']

        valid_modes = ["update_user", 'get_user']

        if mode not in valid_modes:
            raise OS_MissingParam("mode")
        user_id = None
        if "id" in login_options['user']:
            user_id = login_options['user']['id']
        elif "userid" in login_options['user']:
            user_id = login_options['user']['userid']

        user_data = {}
        if "email" in login_options['user']:
            user_data['email'] = login_options['user']['email']
        if "userid" in login_options['user']:
            user_data['userid'] = login_options['user']['userid']

        if "password" in login_options['user']:
            user_data['password'] = login_options['user']['password']
        send_data = {'mode': mode, 'session_key': login_options['session_key'], 'user': user_data, 'userid': login_options['userid']}

        session_data = self.test_user_session(send_data['session_key'], send_data['userid'])
        if not session_data["valid"]:
            raise OS_Auth_InvalidCredentials()

        if not session_data["admin"]:
            if "email" in user_data and user_data["email"] != session_data["session_user"]["email"]:
                raise OS_Auth_InvalidCredentials()
            if "userid" in user_data and user_data["userid"] != session_data["session_user"]["id"]:
                raise OS_Auth_InvalidCredentials()
        
        params = json.dumps(send_data)
        
        headers = {
            "Content-type": "application/json",
            "Accept": "text/plain",
            "APIKey": self.BACKEND_PRIVATE_APIKEY
        }
        
        conn = http.client.HTTPConnection(self.USER_MGR_SERVER)

        conn.request("POST", self.USER_MGR_SCRIPT, params, headers)
        response = json.loads(conn.getresponse().read())

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

        

        try:
            response = self.process_request(request_body)
        except OS_BaseException as e:
            response = e.to_dict()
        except Exception as error:
            response = {"error": repr(error)}

        #if 'error' in response:
        #    start_response('400 BAD REQUEST', [('Content-Type','application/json')])
        #else:
        start_response('200 OK', [('Content-Type','application/json')])


        return response
