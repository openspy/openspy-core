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

class OS_WebUserMgr(BaseService):
    def test_required_params(self, input, params):
        for param in params:
            if param not in input:
                print("{} not in {}".format(param, input))
                return False

        return True

    def test_user_session(self, session_key, userid):
        send_data = {'session_key': session_key, 'userid': userid, 'mode': 'test_session'}
        params = jwt.encode(send_data, self.SECRET_AUTH_KEY, algorithm='HS256')
        
        headers = {"Content-type": "application/x-www-form-urlencoded","Accept": "text/plain"}
        conn = httplib.HTTPConnection(self.LOGIN_SERVER)

        conn.request("POST", self.LOGIN_SCRIPT, params, headers)
        response = conn.getresponse().read()
        response = jwt.decode(response, self.SECRET_AUTH_KEY, algorithm='HS256')

        return response['valid']
    def process_request(self, login_options):
        required_params = ['mode', 'user', 'session_key']
        required_user_params = ['email', 'id']


        if not self.test_required_params(login_options, required_params):            
            return {'error': 'MISSING_REQUIRED_PARAMS_1'}

        if not self.test_required_params(login_options['user'], required_user_params):
            return {'error': 'MISSING_REQUIRED_PARAMS_2'}


        mode = login_options['mode']

        valid_modes = ["update_user", 'get_user']

        if mode not in valid_modes:
            return {'error': 'INVALID_OPERATION_MODE'}

        user_data = {'id': login_options['user']['id'], 'email': login_options['user']['email']}
        if "password" in login_options['user']:
            user_data['password'] = login_options['user']['password']
        send_data = {'mode': mode, 'session_key': login_options['session_key'], 'user': user_data}

        if not self.test_user_session(send_data['session_key'], user_data['id']):
            return {'error': 'INVALID_SESSION'}
        
        params = jwt.encode(send_data, self.SECRET_USERMGR_KEY, algorithm='HS256')
        
        
        headers = {"Content-type": "application/x-www-form-urlencoded","Accept": "text/plain"}

        conn = httplib.HTTPConnection(self.USER_MGR_SERVER)

        conn.request("POST", self.USER_MGR_SCRIPT, params, headers)
        response = conn.getresponse().read()
        response = jwt.decode(response, self.SECRET_USERMGR_KEY, algorithm='HS256')

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

        

        response = json.dumps(self.process_request(request_body))

        if 'error' in response:
            start_response('400 BAD REQUEST', [('Content-Type','text/html')])
        else:
            start_response('200 OK', [('Content-Type','text/html')])


        return response
