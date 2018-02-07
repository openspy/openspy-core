class BaseService():
	def __init__(self):
		self.LOGIN_SERVER = 'localhost:8000'
		self.LOGIN_SCRIPT = '/backend/auth'

		self.REGISTER_SERVER = 'localhost:8000'
		self.REGISTER_SCRIPT = '/backend/register'

		self.USER_MGR_SERVER = 'localhost:8000'
		self.USER_MGR_SCRIPT = '/backend/useraccount'

		self.PROFILE_MGR_SERVER = 'localhost:8000'
		self.PROFILE_MGR_SCRIPT = '/backend/userprofile'

		self.GAME_MGR_SERVER = 'localhost:8000'
		self.GAME_MGR_SCRIPT = '/backend/gameservice'

		self.BACKEND_PRIVATE_APIKEY = "d8ba81d9-f3b2-448b-a36e-f6116338fa5f"
	def sendEmail(self, email_data):
		print("Send email: {}\n".format(email_data))
		return False
