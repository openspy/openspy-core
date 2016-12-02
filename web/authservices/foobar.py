from cgi import parse_qs, escape
import xml.etree.ElementTree as ET

import binascii
from M2Crypto import RSA, BIO
import md5, struct, os

from collections import OrderedDict


LOGIN_RESPONSE_SUCCESS = 0
LOGIN_RESPONSE_SERVERINITFAILED = 1
LOGIN_RESPONSE_USER_NOT_FOUND = 2
LOGIN_RESPONSE_INVALID_PASSWORD = 3
LOGIN_RESPONSE_INVALID_PROFILE = 4
LOGIN_RESPONSE_UNIQUE_NICK_EXPIRED = 5
LOGIN_RESPONSE_DB_ERROR = 6
LOGIN_RESPONSE_SERVER_ERROR = 7

private_key =  "-----BEGIN RSA PRIVATE KEY-----\n" +\
"MIICXQIBAAKBgQDEDRjQu5OOTXpg1IJfHYVhnJxGRLQWLvFCIIVjULZ2MU07uAta\n" +\
"wuOwQAEUt/ZCm0T14brZ8rsJfR3V3OJxrEUEb6TsIPDIl85fL3xctlITVe9ORpPq\n" +\
"Ickg9Q2GXPHvNaeBMyzygQ8sCyTJ62SxT1FmNA3xs08osENXPGIEUaJ8OwIDAQAB\n" +\
"AoGABXisGZ8yhgUphixIGylywH+jaN6f/AKBXywTLOtivDeyBRmkz3qi6hdPMGnV\n" +\
"6JP2v7n2AgEhMSmZvI82jp+VKXcDsbcupZMp4NqFSC03Li2cye5aoFHBWKMkLHfF\n" +\
"TnfvvaO5+vra2M56l2HpPfoMEyOk9OJ/FlepxIpXxNCZQVkCQQDqdy9H8AnXVs+G\n" +\
"/EulUefCx31Ksla3Lf8+JLJOerfJBES3n8/0mRqvtNvSNHcQWUuqyLzB5hYQaEnq\n" +\
"LV1HYavFAkEA1g6z6DVoI4AYxSywR4mz7o8JOxF9hzTCv50pKhIgb4wzdZX+dc+N\n" +\
"4ePXoiD8nrMS4Jixc03NPFLV4O3OmW4H/wJBALXQXjWmibsWci72jaJQ9SsxjpLR\n" +\
"4DSD0p3ZzvrUZpfWW4MYxiWiY/NEiAFk9b8Tv31b1CN3zDxE4qxZKTAlKRECQHAc\n" +\
"0UN8vWdijxauekFtsQzwY6BJX9qx2pJraQT863ohD0612cmwhJpcMDNdXZJtLiTu\n" +\
"NHq0tBq1NAoT45Jem9cCQQCgu2P7MCwKJP9ZUMLKFq7G+Zlqeb9ENtDQICjEfPvP\n" +\
"HyvTGVsGf0jSgo5DK78oGVDbcaazvCQAe2EzXfh2KzgL\n" +\
"-----END RSA PRIVATE KEY-----"

auth_user_dir = OrderedDict()

def generate_signature(rsa, length, version, auth_user_dir, peerkey, server_data, use_md5):
    if use_md5:
        validity_sig = md5.new()
        validity_sig.update(struct.pack("I", length))
        validity_sig.update(struct.pack("I", version))
        validity_sig.update(struct.pack("I", int(auth_user_dir['partnercode'])))
        validity_sig.update(struct.pack("I", int(auth_user_dir['namespaceid'])))
        validity_sig.update(struct.pack("I", int(auth_user_dir['userid'])))
        validity_sig.update(struct.pack("I", int(auth_user_dir['profileid'])))
        validity_sig.update(struct.pack("I", int(auth_user_dir['expiretime'])))
        validity_sig.update(auth_user_dir['profilenick'])
        validity_sig.update(auth_user_dir['uniquenick'])
        validity_sig.update(auth_user_dir['cdkeyhash'])
        validity_sig.update(binascii.unhexlify(peerkey['modulus']))
        validity_sig.update(binascii.unhexlify(peerkey['exponent']))
        validity_sig.update(server_data)
        md5_data = validity_sig.digest()
        return binascii.hexlify(rsa.sign(md5_data, 'md5')).upper()    
    return None


def get_user_by_id(id):
    ret = OrderedDict()
    ret['partnercode'] = '777'
    ret['namespaceid'] = '777'
    ret['userid'] = '10000'
    ret['profileid'] = '10000'
    ret['expiretime'] = '10000'
    ret['profilenick'] = 'Bobbeh'
    ret['uniquenick'] = 'Bobby'
    ret['cdkeyhash'] = '9f86d081884c7d659a2feaa0c55ad015'.upper()

    return ret


def handle_remoteauth_login(xml_tree, rsa):
    resp_xml = ET.Element('SOAP-ENV:Envelope')
    body = ET.SubElement(resp_xml, 'SOAP-ENV:Body')
    login_result = ET.SubElement(body, 'ns1:LoginUniqueNickResult')

    response_code_node = ET.SubElement(login_result, 'ns1:responseCode')
    response_code_node.text = str(LOGIN_RESPONSE_SERVERINITFAILED)
    return resp_xml

def handle_profile_login(xml_tree, rsa):
    resp_xml = ET.Element('SOAP-ENV:Envelope')
    body = ET.SubElement(resp_xml, 'SOAP-ENV:Body')
    login_result = ET.SubElement(body, 'ns1:LoginProfileResult')

    response_code_node = ET.SubElement(login_result, 'ns1:responseCode')
    response_code_node.text = str(LOGIN_RESPONSE_SUCCESS)


    #auth stuff

    certificate_node = ET.SubElement(login_result, 'ns1:certificate')

    peerkeyprivate_node = ET.SubElement(login_result, 'ns1:peerkeyprivate')
    peerkeyprivate_node.text = '0'

    length_node = ET.SubElement(certificate_node, 'ns1:length') #???
    length_node.text = '111'

    version_node = ET.SubElement(certificate_node, 'ns1:version')
    version_node.text = '1'

    auth_user_dir = get_user_by_id(111)
    #user info
    for k,v in auth_user_dir.items():
        node = ET.SubElement(certificate_node, 'ns1:{}'.format(k))
        node.text = v

    #encrypted server data
    peerkeymodulus_node = ET.SubElement(certificate_node, 'ns1:peerkeymodulus')
    rsa_modulus = binascii.hexlify(rsa.n)
    peerkeymodulus_node.text = rsa_modulus[-128:].upper()

    peerkeyexponent_node = ET.SubElement(certificate_node, 'ns1:peerkeyexponent')
    rsa_exponent = binascii.hexlify(rsa.e)
    peerkeyexponent_node.text = rsa_exponent[-6:]

    serverdata_node = ET.SubElement(certificate_node, 'ns1:serverdata')

    server_data = os.urandom(128)
    serverdata_node.text = binascii.hexlify(server_data)

    signature_node = ET.SubElement(certificate_node, 'ns1:signature')

    peerkey = {}
    peerkey['exponent'] = peerkeyexponent_node.text
    peerkey['modulus'] = peerkeymodulus_node.text
    signature_node.text = generate_signature(rsa, int(length_node.text), int(version_node.text), auth_user_dir, peerkey, server_data, True)

    profilenick_node = xml_tree.find('{http://gamespy.net/AuthService/}profilenick')
    profilenick = profilenick_node.text

    email_node = xml_tree.find('{http://gamespy.net/AuthService/}email')
    email = email_node.text

    partnercode_node = xml_tree.find('{http://gamespy.net/AuthService/}partnercode')
    partnercode = partnercode_node.text

    namespaceid_node = xml_tree.find('{http://gamespy.net/AuthService/}partnercode')
    namespaceid = namespaceid_node.text

    encrypted_pass = xml_tree.find('{http://gamespy.net/AuthService/}password').find('{http://gamespy.net/AuthService/}Value')
    password = rsa.private_decrypt(binascii.unhexlify(encrypted_pass.text), RSA.pkcs1_padding) 
    print("Decrypted pass: {}".format(password))

    return resp_xml


def handle_unique_login(xml_tree, rsa):
    resp_xml = ET.Element('SOAP-ENV:Envelope')
    body = ET.SubElement(resp_xml, 'SOAP-ENV:Body')
    login_result = ET.SubElement(body, 'ns1:LoginUniqueNickResult')

    response_code_node = ET.SubElement(login_result, 'ns1:responseCode')
    response_code_node.text = str(LOGIN_RESPONSE_SUCCESS)


    #auth stuff

    certificate_node = ET.SubElement(login_result, 'ns1:certificate')

    peerkeyprivate_node = ET.SubElement(login_result, 'ns1:peerkeyprivate')
    peerkeyprivate_node.text = '0'

    length_node = ET.SubElement(certificate_node, 'ns1:length') #???
    length_node.text = '111'

    version_node = ET.SubElement(certificate_node, 'ns1:version')
    version_node.text = '1'

    auth_user_dir = get_user_by_id(111)
    #user info
    for k,v in auth_user_dir.items():
        node = ET.SubElement(certificate_node, 'ns1:{}'.format(k))
        node.text = v

    #encrypted server data
    peerkeymodulus_node = ET.SubElement(certificate_node, 'ns1:peerkeymodulus')
    rsa_modulus = binascii.hexlify(rsa.n)
    peerkeymodulus_node.text = rsa_modulus[-128:].upper()

    peerkeyexponent_node = ET.SubElement(certificate_node, 'ns1:peerkeyexponent')
    rsa_exponent = binascii.hexlify(rsa.e)
    peerkeyexponent_node.text = rsa_exponent[-6:]

    serverdata_node = ET.SubElement(certificate_node, 'ns1:serverdata')

    server_data = os.urandom(128)
    serverdata_node.text = binascii.hexlify(server_data)

    signature_node = ET.SubElement(certificate_node, 'ns1:signature')

    peerkey = {}
    peerkey['exponent'] = peerkeyexponent_node.text
    peerkey['modulus'] = peerkeymodulus_node.text
    signature_node.text = generate_signature(rsa, int(length_node.text), int(version_node.text), auth_user_dir, peerkey, server_data, True)

    uniquenick_node = xml_tree.find('{http://gamespy.net/AuthService/}uniquenick')
    uniquenick = uniquenick_node.text

    partnercode_node = xml_tree.find('{http://gamespy.net/AuthService/}partnercode')
    partnercode = partnercode_node.text

    namespaceid_node = xml_tree.find('{http://gamespy.net/AuthService/}partnercode')
    namespaceid = namespaceid_node.text

    
    encrypted_pass = xml_tree.find('{http://gamespy.net/AuthService/}password').find('{http://gamespy.net/AuthService/}Value')
    password = rsa.private_decrypt(binascii.unhexlify(encrypted_pass.text), RSA.pkcs1_padding) 
    print("Decrypted pass: {}".format(password))

    return resp_xml


def get_pass(*args):
        return str('123321')

def application(env, start_response):
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

    start_response('200 OK', [('Content-Type','text/html')])

    mapping_file = open("post_data.txt","a")

    bio = BIO.MemoryBuffer(private_key)
    
    rsa = RSA.load_key_bio(bio, get_pass)
    
    tree = ET.ElementTree(ET.fromstring(request_body))

    login_profile_tree = tree.find('.//{http://gamespy.net/AuthService/}LoginProfile')
    login_remoteauth_tree = tree.find('.//{http://gamespy.net/AuthService/}LoginRemoteAuth')
    login_uniquenick_tree = tree.find('.//{http://gamespy.net/AuthService/}LoginUniqueNick')
    login_ps3_tree = tree.find('.//{http://gamespy.net/AuthService/}LoginPs3Cert')
    login_facebook_tree = tree.find('.//{http://gamespy.net/AuthService/}LoginFacebook')

    resp = None
    if login_uniquenick_tree != None:
        resp = handle_unique_login(login_uniquenick_tree, rsa)
    elif login_profile_tree != None:
        mapping_file.write("{}\n".format(ET.tostring(login_profile_tree, encoding='utf8', method='xml')))
        resp = handle_profile_login(login_profile_tree, rsa)
    elif login_remoteauth_tree != None:
        resp = handle_remoteauth_login(login_remoteauth_tree, rsa)

    #priv_key_file = open('private_key.pem','rb')
    #rsa = RSA.importKey(priv_key_file.read(),passphrase='123321')
    #mapping_file.write("{}\n".format(rsa))

    if resp != None:
        return ET.tostring(resp, encoding='utf8', method='xml')
    return [b"Hello World"]
