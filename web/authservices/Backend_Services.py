from cgi import parse_qs, escape

from BaseService import BaseService
#from AccountService.AuthService import AuthService
#from AccountService.RegistrationService import RegistrationService
#import AccountService.AuthService as AuthService
from AccountService.AuthService import AuthService
import AccountService.RegistrationService as RegistrationService
from public.GS_AuthService import GS_AuthService
from public.OS_WebAuth import OS_WebAuth
import re
import json
#import RegistrationService from RegistrationService

def application(env, start_response):
	path_split = re.split('/', env.get('REQUEST_URI'))
	if path_split[1] == "backend":
		#TODO: backend security check
		if path_split[2] == "auth":
			service = AuthService()
		elif path_split[2] == "register":
			service = RegistrationService()
	#non-backend stuff, publically accessible services
	elif path_split[1] == "AuthService":
		if path_split[2] == "AuthService.asmx":
			service = GS_AuthService()
	elif path_split[1] == "web":
		if path_split[2] == "auth":
			service = OS_WebAuth()
	
	if service == None:
		start_response('400 BAD REQUEST', [('Content-Type','text/html')])
		return [b'Bad Request']
	return service.run(env, start_response)