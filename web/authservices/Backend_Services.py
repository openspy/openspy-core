from cgi import parse_qs, escape

from BaseService import BaseService
import AccountService.AuthService as AuthService
from AccountService.AuthService import AuthService
from AccountService.UserAccountMgrService import UserAccountMgrService
from AccountService.RegistrationService import RegistrationService
from AccountService.UserProfileMgrService import UserProfileMgrService
from public.GS_AuthService import GS_AuthService
from public.OS_WebAuth import OS_WebAuth
from public.OS_WebUserMgr import OS_WebUserMgr
from public.OS_WebProfileMgr import OS_WebProfileMgr
import re
import json
#import RegistrationService from RegistrationService

def application(env, start_response):

	path_split = re.split('/', env.get('REQUEST_URI'))
	service = None
	if path_split[1] == "backend":
		#TODO: backend security check
		if path_split[2] == "auth":
			service = AuthService()
		elif path_split[2] == "register":
			service = RegistrationService()
		elif path_split[2] == "useraccount":
			service = UserAccountMgrService()
		elif path_split[2] == "userprofile":
			service = UserProfileMgrService()


	#the paths containing "web" are the publically accessable interfaces which theoretically can be accessed by any user anonymously, however typically is as an interface via another web backend
	#these services are responsible for checking the session key and testing that it is valid for the user which is trying to make changes/update the account/profiles in question, and 
	#are responsible for sanitizing all input before it goes to the backend, as the backend performs no permission checks and simply acts as an interface to perform the requested changes
	#the backend scripts should be denied by default and only accessible via IP whitelisting of servers that need to access it
	elif path_split[1] == "web":
		if path_split[2] == "auth":
			service = OS_WebAuth()
		elif path_split[2] == "usermgr":
			service = OS_WebUserMgr()
		elif path_split[2] == "profilemgr":
			service = OS_WebProfileMgr()

	#GameSpy SDK files, publically accessable
	elif path_split[1] == "AuthService":
		if path_split[2] == "AuthService.asmx":
			service = GS_AuthService()
	
	if service == None:
		start_response('400 BAD REQUEST', [('Content-Type','text/html')])
		return [b'Bad Request']

	print("Running service: {}".format(service))
	return service.run(env, start_response)