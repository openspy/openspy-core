class BaseService():
	def __init__(self):
		self.LOGIN_SERVER = 'localhost:8000'
		self.LOGIN_SCRIPT = '/backend/auth'
		self.SECRET_AUTH_KEY = "dGhpc2lzdGhla2V5dGhpc2lzdGhla2V5dGhpc2lzdGhla2V5"

		self.REGISTER_SERVER = 'localhost:8000'
		self.REGISTER_SCRIPT = '/backend/register'
		self.SECRET_REGISTER_KEY = "dGhpc2lzdGhla2V5dGhpc2lzdGhla2V5dGhpc2lzdGhla2V5"

		self.USER_MGR_SERVER = 'localhost:8000'
		self.USER_MGR_SCRIPT = '/backend/useraccount'
		self.SECRET_USERMGR_KEY = "dGhpc2lzdGhla2V5dGhpc2lzdGhla2V5dGhpc2lzdGhla2V5"

		self.PROFILE_MGR_SERVER = 'localhost:8000'
		self.PROFILE_MGR_SCRIPT = '/backend/userprofile'
		self.SECRET_PROFILEMGR_KEY = "dGhpc2lzdGhla2V5dGhpc2lzdGhla2V5dGhpc2lzdGhla2V5"

		self.SECRET_PERSIST_KEY = "dGhpc2lzdGhla2V5dGhpc2lzdGhla2V5dGhpc2lzdGhla2V5"

	def sendEmail(self, email_data):
		print("Send email: {}\n".format(email_data))
		return False
