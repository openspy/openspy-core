from cgi import parse_qs, escape

import jwt

import MySQLdb
import uuid

from BaseModel import BaseModel
from User import User #openspy user


SECRET_AUTH_KEY = "dGhpc2lzdGhla2V5dGhpc2lzdGhla2V5dGhpc2lzdGhla2V5"

LOGIN_RESPONSE_SUCCESS = 0
LOGIN_RESPONSE_SERVERINITFAILED = 1
LOGIN_RESPONSE_USER_NOT_FOUND = 2
LOGIN_RESPONSE_INVALID_PASSWORD = 3
LOGIN_RESPONSE_INVALID_PROFILE = 4
LOGIN_RESPONSE_UNIQUE_NICK_EXPIRED = 5
LOGIN_RESPONSE_DB_ERROR = 6
LOGIN_RESPONSE_SERVER_ERROR = 7






def build_get_profile():
    ret = "SELECT u.id,p.profileid,u.email,p.uniquenick, p.nick from profiles p inner join users u on u.id = p.userid"
    return ret

def get_profile_by_uniquenick(db_ctx, uniquenick, namespaceid, partnercode):
    query_str = build_get_profile()
    cursor = db_ctx.cursor()
    query_str += ' where p.uniquenick = %s and p.deleted = 0'

    cursor.execute(query_str, (uniquenick))
    profile = {}
    for (userid, profileid, email, uniquenick, nick) in cursor:
        profile['userid'] = int(userid)
        profile['profileid'] = int(profileid)
        profile['email'] = email
        profile['uniquenick'] = uniquenick
        profile['profilenick'] = nick
        profile['namespaceid'] = int(namespaceid)
        profile['partnercode'] = int(partnercode)
        profile['expiretime'] = 100000 #TODO: figure out if this value is unix timestamp, offset in seconds/ms, etc in web services sdk
    cursor.close()
    return profile

def get_profile_by_nick_email(db_ctx, nick, email, partnercode):
    query_str = build_get_profile()
    cursor = db_ctx.cursor()
    query_str += ' where p.nick = %s and u.email = %s and p.deleted = 0'

    cursor.execute(query_str, (nick, email))
    profile = {}
    for (userid, profileid, email, uniquenick, nick) in cursor:
        profile['userid'] = int(userid)
        profile['profileid'] = int(profileid)
        profile['email'] = email
        profile['uniquenick'] = uniquenick
        profile['profilenick'] = nick
        profile['namespaceid'] = 0
        profile['partnercode'] = int(partnercode)
        profile['expiretime'] = 100000 #TODO: figure out if this value is unix timestamp, offset in seconds/ms, etc in web services sdk
    cursor.close()
    return profile
def test_pass_plain_by_userid(db_ctx, userid, password):
    auth_success = False
    cursor = db_ctx.cursor()

    cursor.execute('SELECT 1 from users where id = %s AND password = %s and deleted = 0', (userid, password))

    auth_success = cursor.rowcount > 0

    cursor.close()
    return auth_success

def create_auth_session(profile):
    session_key = uuid.uuid1()
    redis_key = '{}:{}:{}'.format(profile['userid'],profile['profileid'],session_key)
    return {'redis_key': redis_key, 'session_key': session_key}

def application(env, start_response):
    db_ctx = MySQLdb.connect(user='root', db='GameTracker')

    the_user = User.get(User.id == 10001)
    print("The user: {} - {}\n".format(the_user,the_user.id))

    # the environment variable CONTENT_LENGTH may be empty or missing
    try:
        request_body_size = int(env.get('CONTENT_LENGTH', 0))
    except (ValueError):
        request_body_size = 0

    response = {}
    response['success'] = False

    # When the method is POST the variable will be sent
    # in the HTTP request body which is passed by the WSGI server
    # in the file like wsgi.input environment variable.
    request_body = env['wsgi.input'].read(request_body_size)
    jwt_decoded = jwt.decode(request_body, SECRET_AUTH_KEY, algorithm='HS256')

    if 'namespaceid' not in jwt_decoded:
        jwt_decoded['namespaceid'] = 0

    hash_type = 'plain'
    if 'hash_type' in jwt_decoded:
        hash_type = jwt_decoded['hash_type']

    if 'password' not in jwt_decoded:
        response['reason'] = LOGIN_RESPONSE_INVALID_PASSWORD
        db_ctx.close()
        return jwt.encode(response, secret_auth_key, algorithm='HS256')

    profile = None
    if 'uniquenick' in jwt_decoded:
        profile = get_profile_by_uniquenick(db_ctx, jwt_decoded['uniquenick'], jwt_decoded['namespaceid'], jwt_decoded['partnercode'])
        if profile == None:
            response['reason'] = LOGIN_RESPONSE_INVALID_PROFILE
    elif 'profilenick' in jwt_decoded and 'email' in jwt_decoded:
        profile = get_profile_by_nick_email(db_ctx, jwt_decoded['profilenick'], jwt_decoded['email'], jwt_decoded['partnercode'])
        if profile == None:
            response['reason'] = LOGIN_RESPONSE_INVALID_PROFILE
    else:
        response['reason'] = LOGIN_RESPONSE_SERVER_ERROR
        db_ctx.close()
        return jwt.encode(response, SECRET_AUTH_KEY, algorithm='HS256')

    auth_success = False
    if hash_type == 'plain':
        print("User: {}\n".format(profile))
        auth_success = test_pass_plain_by_userid(db_ctx, profile['userid'], jwt_decoded['password'])

    if not auth_success:
        response['reason'] = LOGIN_RESPONSE_INVALID_PASSWORD
    else:
        response['success'] = True
        response['profile'] = profile


    print("JWT Resp: {}\n".format(response))

    db_ctx.close()

    return jwt.encode(response, SECRET_AUTH_KEY, algorithm='HS256')
