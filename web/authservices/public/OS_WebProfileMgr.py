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

class OS_WebProfileMgr(BaseService):
    def test_required_params(self, input, params):
        for param in params:
            if param not in input:
                return False

        return True

    def test_profile_ownership(self, session_key, profileid):
        send_data = {'session_key': session_key, 'profileid': profileid, 'mode': 'test_session_profileid'}
        params = jwt.encode(send_data, self.SECRET_AUTH_KEY, algorithm='HS256')
        
        headers = {"Content-type": "application/x-www-form-urlencoded","Accept": "text/plain"}
        conn = httplib.HTTPConnection(self.LOGIN_SERVER)

        conn.request("POST", self.LOGIN_SCRIPT, params, headers)
        response = conn.getresponse().read()
        response = jwt.decode(response, self.SECRET_AUTH_KEY, algorithm='HS256')
        return response['valid']

    def test_user_session(self, session_key, userid):
        send_data = {'session_key': session_key, 'userid': userid, 'mode': 'test_session'}
        params = jwt.encode(send_data, self.SECRET_AUTH_KEY, algorithm='HS256')
        
        headers = {"Content-type": "application/x-www-form-urlencoded","Accept": "text/plain"}
        conn = httplib.HTTPConnection(self.LOGIN_SERVER)

        conn.request("POST", self.LOGIN_SCRIPT, params, headers)
        response = conn.getresponse().read()
        response = jwt.decode(response, self.SECRET_AUTH_KEY, algorithm='HS256')
        return response['valid']

    def handle_update_profile(self, data):

        passthrough_params = ["session_key"]
        passthrough_profile_params = ["uniquenick", "nick", "id"]

        profile = {}
        for key in passthrough_profile_params:
            if key in data["profile"]:
                profile[key] = data["profile"][key]
        send_data = {'mode': 'update_profiles', 'profile': profile, 'session_key': data['session_key']}

        #update_profiles

        params = jwt.encode(send_data, self.SECRET_PROFILEMGR_KEY, algorithm='HS256')
        
        
        headers = {"Content-type": "application/x-www-form-urlencoded","Accept": "text/plain"}

        conn = httplib.HTTPConnection(self.PROFILE_MGR_SERVER)

        conn.request("POST", self.PROFILE_MGR_SCRIPT, params, headers)
        response = conn.getresponse().read()
        response = jwt.decode(response, self.SECRET_PROFILEMGR_KEY, algorithm='HS256')


        return response
    def handle_get_profiles(self, data):

        if "session_key" not in data or "userid" not in data:
            return False
        if not self.test_user_session(data["session_key"], data["userid"]):
            return {'error': 'INVALID_SESSION'}
        request_data = {'session_key': data['session_key'], 'userid': data['userid'], 'mode': 'get_profiles'}

        params = jwt.encode(request_data, self.SECRET_PROFILEMGR_KEY, algorithm='HS256')
        
        
        headers = {"Content-type": "application/x-www-form-urlencoded","Accept": "text/plain"}

        conn = httplib.HTTPConnection(self.PROFILE_MGR_SERVER)

        conn.request("POST", self.PROFILE_MGR_SCRIPT, params, headers)
        response = conn.getresponse().read()
        response = jwt.decode(response, self.SECRET_PROFILEMGR_KEY, algorithm='HS256')

        return response

    def process_request(self, data):

        if "mode" not in data:
            return {'error': 'INVALID_MODE'}

        print("WebProfileMgr: {}\n".format(data))

        has_ownership = False
        if "profile" in data:
            has_ownership = self.test_profile_ownership(data["session_key"], data["profile"]["id"])
        elif "userid" in data:
            has_ownership = self.test_user_session(data["session_key"], data["userid"])
        if not has_ownership:
            return {'error': 'INVALID_PROFILEID'}

        if data["mode"] == "update_profile":
            return self.handle_update_profile(data)
        elif data["mode"] == "get_profiles":
            return self.handle_get_profiles(data)
        else:
            return {'error': 'INVALID_MODE'}
        
        
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
