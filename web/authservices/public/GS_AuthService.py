from cgi import parse_qs, escape
import xml.etree.ElementTree as ET

import binascii
import hashlib

from collections import OrderedDict

import simplejson as json
import http.client
import struct, os
import rsa

from BaseService import BaseService

class GS_AuthService(BaseService):
    LOGIN_RESPONSE_SUCCESS = 0
    LOGIN_RESPONSE_SERVERINITFAILED = 1
    LOGIN_RESPONSE_USER_NOT_FOUND = 2
    LOGIN_RESPONSE_INVALID_PASSWORD = 3
    LOGIN_RESPONSE_INVALID_PROFILE = 4
    LOGIN_RESPONSE_UNIQUE_NICK_EXPIRED = 5
    LOGIN_RESPONSE_DB_ERROR = 6
    LOGIN_RESPONSE_SERVER_ERROR = 7


    def generate_signature(self, privkey, length, version, auth_user_dir, peerkey, server_data, use_md5):
        buffer = struct.pack("I", length)
        buffer += struct.pack("I", version)

        buffer +=  struct.pack("I", int(auth_user_dir['partnercode']))
        buffer += struct.pack("I", int(auth_user_dir['namespaceid']))
        buffer += struct.pack("I", int(auth_user_dir['userid']))
        buffer += struct.pack("I", int(auth_user_dir['profileid']))
        buffer += struct.pack("I", int(auth_user_dir['expiretime']))
        buffer += auth_user_dir['profilenick'].encode('utf8')
        buffer += auth_user_dir['uniquenick'].encode('utf8')
        buffer += auth_user_dir['cdkeyhash'].encode('utf8')

        buffer += binascii.unhexlify(peerkey['modulus'])

        buffer += binascii.unhexlify("0{}".format(peerkey['exponent']))
        buffer += server_data

        hash_algo = 'MD5'
        if not use_md5:
            hash_algo = 'SHA-1'
        sig_key = rsa.sign(buffer, privkey, hash_algo)
        key = sig_key.upper()
        key = binascii.hexlify(sig_key).decode('utf8').upper()    

        return key


    #will be used if LOGIN_RESPONE_* differs from backend auth
    def convert_reason_code(self, reason):
        return str(reason)

    def try_authenticate(self, login_data):
        
        login_data["user_login"] = False
        login_data["profile_login"] = False

        params = json.dumps(login_data)
      
        headers = {"Content-type": "application/json","Accept": "text/plain"}
        conn = http.client.HTTPConnection(self.LOGIN_SERVER)

        conn.request("POST", self.LOGIN_SCRIPT, params, headers)
        response = json.loads(conn.getresponse().read())
   
        if "success" in response and response["success"] == True:
            profile = response['profile']
            response['profile'] = OrderedDict()
            
            response['profile']['partnercode'] = profile['user']['partnercode']
            response['profile']['namespaceid'] = profile['namespaceid']
            response['profile']['userid'] = profile['user']['id']
            response['profile']['profileid'] = profile['id']
            response['profile']['expiretime'] = response['expiretime']
            response['profile']['profilenick'] = profile['nick']
            response['profile']['uniquenick'] = profile['uniquenick']
            response['profile']['cdkeyhash'] = '9f86d081884c7d659a2feaa0c55ad015'.upper()
            return response

        #failed, delete profile if exists and send error
        if 'profile' in response:
            del response['profile']

        return response


    def handle_remoteauth_login(self, xml_tree, rsa):
        resp_xml = ET.Element('SOAP-ENV:Envelope')
        body = ET.SubElement(resp_xml, 'SOAP-ENV:Body')
        login_result = ET.SubElement(body, 'ns1:LoginRemoteAuthResult')

        response_code_node = ET.SubElement(login_result, 'ns1:responseCode')
        response_code_node.text = str(self.LOGIN_RESPONSE_SERVERINITFAILED)
        return resp_xml

    def handle_profile_login(self, xml_tree, privkey):
        resp_xml = ET.Element('SOAP-ENV:Envelope')
        body = ET.SubElement(resp_xml, 'SOAP-ENV:Body')
        login_result = ET.SubElement(body, 'ns1:LoginProfileResult')

        response_code_node = ET.SubElement(login_result, 'ns1:responseCode')
        
        #auth stuff

        certificate_node = ET.SubElement(login_result, 'ns1:certificate')

        peerkeyprivate_node = ET.SubElement(login_result, 'ns1:peerkeyprivate')
        peerkeyprivate_node.text = '0'

        length_node = ET.SubElement(certificate_node, 'ns1:length') #???
        length_node.text = '111'

        version_node = ET.SubElement(certificate_node, 'ns1:version')
        version_node.text = '1'

        #get login info
        profilenick_node = xml_tree.find('{http://gamespy.net/AuthService/}profilenick')
        profilenick = profilenick_node.text

        email_node = xml_tree.find('{http://gamespy.net/AuthService/}email')
        email = email_node.text

        partnercode_node = xml_tree.find('{http://gamespy.net/AuthService/}partnercode')
        partnercode = partnercode_node.text

        namespaceid_node = xml_tree.find('{http://gamespy.net/AuthService/}namespaceid')
        namespaceid = namespaceid_node.text

        #decrypt pw
        encrypted_pass = xml_tree.find('{http://gamespy.net/AuthService/}password').find('{http://gamespy.net/AuthService/}Value')
        password = rsa.decrypt(binascii.unhexlify(encrypted_pass.text),privkey).decode('utf8')

        params = {"hash_type": "plain"}
        params["user"] = {'email': email, 'password': password, 'partnercode': int(partnercode)}
        params["profile"] = {"nick": profilenick, "namespaceid": int(namespaceid)}

        auth_user_dir = self.try_authenticate(params)

        if 'profile' in auth_user_dir:
            response_code_node.text = str(self.LOGIN_RESPONSE_SUCCESS)
            #populate user info
            for k,v in auth_user_dir['profile'].items():
                node = ET.SubElement(certificate_node, 'ns1:{}'.format(k))
                node.text = str(v)
        else: #send error data
            response_code_node.text = self.convert_reason_code(auth_user_dir['reason'])


        #encrypted server data
        peerkeymodulus_node = ET.SubElement(certificate_node, 'ns1:peerkeymodulus')

        rsa_modulus = str(privkey.n)
        peerkeymodulus_node.text = rsa_modulus[-128:].upper()

        peerkeyexponent_node = ET.SubElement(certificate_node, 'ns1:peerkeyexponent')
        rsa_exponent = hex(privkey.e)
        peerkeyexponent_node.text = rsa_exponent[2:]

        serverdata_node = ET.SubElement(certificate_node, 'ns1:serverdata')

        server_data = os.urandom(128)
        serverdata_node.text = binascii.hexlify(server_data).decode('utf8')

        signature_node = ET.SubElement(certificate_node, 'ns1:signature')

        peerkey = {}
        peerkey['exponent'] = peerkeyexponent_node.text
        peerkey['modulus'] = peerkeymodulus_node.text

        signature_node.text = self.generate_signature(privkey, int(length_node.text), int(version_node.text), auth_user_dir['profile'], peerkey, server_data, True)

        return resp_xml


    def handle_unique_login(self, xml_tree, privkey):
        resp_xml = ET.Element('SOAP-ENV:Envelope')
        body = ET.SubElement(resp_xml, 'SOAP-ENV:Body')
        login_result = ET.SubElement(body, 'ns1:LoginUniqueNickResult')

        response_code_node = ET.SubElement(login_result, 'ns1:responseCode')


        #auth stuff

        certificate_node = ET.SubElement(login_result, 'ns1:certificate')

        peerkeyprivate_node = ET.SubElement(login_result, 'ns1:peerkeyprivate')
        peerkeyprivate_node.text = '0'

        length_node = ET.SubElement(certificate_node, 'ns1:length') #???
        length_node.text = '111'

        version_node = ET.SubElement(certificate_node, 'ns1:version')
        version_node.text = '1'

        
        #get request info
        uniquenick_node = xml_tree.find('{http://gamespy.net/AuthService/}uniquenick')
        uniquenick = uniquenick_node.text

        partnercode_node = xml_tree.find('{http://gamespy.net/AuthService/}partnercode')
        partnercode = partnercode_node.text

        namespaceid_node = xml_tree.find('{http://gamespy.net/AuthService/}partnercode')
        namespaceid = namespaceid_node.text

        #decrypt pw
        encrypted_pass = xml_tree.find('{http://gamespy.net/AuthService/}password').find('{http://gamespy.net/AuthService/}Value')
        password = rsa.decrypt(binascii.unhexlify(encrypted_pass.text),privkey)

        params = {"hash_type": "plain"}
        params["user"] = {'password': password, 'partnercode': int(partnercode)}
        params["profile"] = {"uniquenick": uniquenick, "namespaceid": int(namespaceid)}

        #try auth
        auth_user_dir = self.try_authenticate(params)

        print("Unique: {}\n{}\n".format(auth_user_dir, params))
        

        if 'profile' in auth_user_dir:
            response_code_node.text = str(self.LOGIN_RESPONSE_SUCCESS)
            #populate user info
            for k,v in auth_user_dir['profile'].items():
                node = ET.SubElement(certificate_node, 'ns1:{}'.format(k))
                node.text = str(v)
        else: #send error data
            response_code_node.text = self.convert_reason_code(auth_user_dir['reason'])

        #encrypted server data
        peerkeymodulus_node = ET.SubElement(certificate_node, 'ns1:peerkeymodulus')
        rsa_modulus = str(privkey.n)
        peerkeymodulus_node.text = rsa_modulus[-128:].upper()

        peerkeyexponent_node = ET.SubElement(certificate_node, 'ns1:peerkeyexponent')
        rsa_exponent = str(privkey.e)
        peerkeyexponent_node.text = rsa_exponent[-6:]

        serverdata_node = ET.SubElement(certificate_node, 'ns1:serverdata')

        server_data = os.urandom(128)
        serverdata_node.text = binascii.hexlify(server_data).decode('utf8')

        signature_node = ET.SubElement(certificate_node, 'ns1:signature')

        peerkey = {}
        peerkey['exponent'] = peerkeyexponent_node.text
        peerkey['modulus'] = peerkeymodulus_node.text
        signature_node.text = (self.generate_signature(privkey, int(length_node.text), int(version_node.text), auth_user_dir['profile'], peerkey, server_data, True))


        return resp_xml


    def get_pass(self, *args):
            return str('123321')

    def run(self, env, start_response):
        # the environment variable CONTENT_LENGTH may be empty or missing
        try:
            request_body_size = int(env.get('CONTENT_LENGTH', 0))
        except (ValueError):
            request_body_size = 0

        # When the method is POST the variable will be sent
        # in the HTTP request body which is passed by the WSGI server
        # in the file like wsgi.input environment variable.
        request_body = env['wsgi.input'].read(request_body_size)
       # d = parse_qs(request_body)
        print("Got: {}\n".format(request_body))
        start_response('200 OK', [('Content-Type','text/html')])

        #print("privkey: {}\n".format(rsa.newkeys(512)))
        private_key_file = open("/home/andy/openspy-core-v2/web/authservices/openspy_webservices_private.pem","r")
        keydata = private_key_file.read()
        privkey = rsa.PrivateKey.load_pkcs1(keydata)

        #bio = BIO.MemoryBuffer(private_key_file.read())
        
        #rsa = RSA.load_key_bio(bio, self.get_pass)
        #rsa = None
        
        tree = ET.ElementTree(ET.fromstring(request_body))

        login_profile_tree = tree.find('.//{http://gamespy.net/AuthService/}LoginProfile')
        login_remoteauth_tree = tree.find('.//{http://gamespy.net/AuthService/}LoginRemoteAuth')
        login_uniquenick_tree = tree.find('.//{http://gamespy.net/AuthService/}LoginUniqueNick')
        login_ps3_tree = tree.find('.//{http://gamespy.net/AuthService/}LoginPs3Cert')
        login_facebook_tree = tree.find('.//{http://gamespy.net/AuthService/}LoginFacebook')

        resp = None
        if login_uniquenick_tree != None:
            resp = self.handle_unique_login(login_uniquenick_tree, privkey)
        elif login_profile_tree != None:
            resp = self.handle_profile_login(login_profile_tree, privkey)
        elif login_remoteauth_tree != None:
            resp = self.handle_remoteauth_login(login_remoteauth_tree, privkey)

        if resp != None:
            print("RET: {}\n".format(ET.tostring(resp, encoding='utf8', method='xml')))
            return ET.tostring(resp, encoding='utf8', method='xml')
        else:
            return 'Fatal error'
