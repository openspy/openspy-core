#include <OS/OpenSpy.h>
#include <OS/GPShared.h>
namespace GPShared {

	const GPStatus gp_default_status = {GP_OFFLINE, "Offline", "", {0,0}, GP_SILENCE_NONE};
	GPErrorData gp_error_data[] = {
		// General error codes.
		{GP_GENERAL, "There was an unknown error. ", true},
		{GP_PARSE, "Unexpected data was received from the server.", true},
		{GP_NOT_LOGGED_IN, "The request cannot be processed because user has not logged in. ", true},
		{GP_BAD_SESSKEY, "The request cannot be processed because of an invalid session key.", true},
		{GP_DATABASE, "There was a database error.", true},
		{GP_NETWORK, "There was an error connecting a network socket.", true},
		{GP_FORCED_DISCONNECT, "This profile has been disconnected by another login.", true},
		{GP_CONNECTION_CLOSED, "The server has closed the connection.", true},
		{GP_UDP_LAYER, "There was a problem with the UDP layer.", true},
		// Error codes that can occur while logging in.
		{GP_LOGIN, "There was an error logging in to the GP backend.", false},
		{GP_LOGIN_TIMEOUT, "The login attempt timed out.", false},
		{GP_LOGIN_BAD_NICK, "The nickname provided was incorrect.", false},
		{GP_LOGIN_BAD_EMAIL, "The e-mail address provided was incorrect.", false},
		{GP_LOGIN_BAD_PASSWORD, "The password provided was incorrect.", false},
		{GP_LOGIN_BAD_PROFILE, "The profile provided was incorrect.", false},
		{GP_LOGIN_PROFILE_DELETED, "The profile has been deleted.", false},
		{GP_LOGIN_CONNECTION_FAILED, "The server has refused the connection.", false},
		{GP_LOGIN_SERVER_AUTH_FAILED, "The server could not be authenticated.", false},
		{GP_LOGIN_BAD_UNIQUENICK, "The uniquenick provided was incorrect.", false},
		{GP_LOGIN_BAD_PREAUTH, "There was an error validating the pre-authentication.", false},
		{GP_LOGIN_BAD_LOGIN_TICKET, "The login ticket was unable to be validated.", false},
		{GP_LOGIN_EXPIRED_LOGIN_TICKET, "The login ticket had expired and could not be used.", false},

		// Error codes that can occur while creating a new user.
		{GP_NEWUSER, "There was an error creating a new user.", false},
		{GP_NEWUSER_BAD_NICK, "A profile with that nick already exists.", false},
		{GP_NEWUSER_BAD_PASSWORD, "The password does not match the email address.", false},
		{GP_NEWUSER_UNIQUENICK_INVALID, "The uniquenick is invalid.", false},
		{GP_NEWUSER_UNIQUENICK_INUSE, "The uniquenick is already in use.", false},

		// Error codes that can occur while updating user information.
		{GP_UPDATEUI, "There was an error updating the user information.", false},
		{GP_UPDATEUI_BAD_EMAIL, "A user with the email address provided already exists.", false},

		// Error codes that can occur while creating a new profile.
		{GP_NEWPROFILE, "There was an error creating a new profile.", false},
		{GP_NEWPROFILE_BAD_NICK, "The nickname to be replaced does not exist.", false},
		{GP_NEWPROFILE_BAD_OLD_NICK, "A profile with the nickname provided already exists.", false},

		// Error codes that can occur while updating profile information.
		{GP_UPDATEPRO, "There was an error updating the profile information. ", false},
		{GP_UPDATEPRO_BAD_NICK, "A user with the nickname provided already exists.", false},

		// Error codes that can occur while creating a new profile.
		{GP_NEWPROFILE,"There was an error creating a new profile.", false},
		{GP_NEWPROFILE_BAD_NICK,"The nickname to be replaced does not exist.", false},
		{GP_NEWPROFILE_BAD_OLD_NICK,"A profile with the nickname provided already exists.", false},

		// Error codes that can occur while updating profile information.
		{GP_UPDATEPRO,"There was an error updating the profile information. ", false},
		{GP_UPDATEPRO_BAD_NICK,"A user with the nickname provided already exists.", false},

		// Error codes that can occur while adding someone to your buddy list.
		{GP_ADDBUDDY,"There was an error adding a buddy. ", false},
		{GP_ADDBUDDY_BAD_FROM,"The profile requesting to add a buddy is invalid.", false},
		{GP_ADDBUDDY_BAD_NEW,"The profile requested is invalid.", false},
		{GP_ADDBUDDY_ALREADY_BUDDY,"The profile requested is already a buddy.", false},
		{GP_ADDBUDDY_IS_ON_BLOCKLIST,"The profile requested is on the local profile's block list.", false},
		//DOM-IGNORE-BEGIN 
		{GP_ADDBUDDY_IS_BLOCKING,"Reserved for future use.", false},
		//DOM-IGNORE-END

		// Error codes that can occur while being authorized to add someone to your buddy list.
		{GP_AUTHADD,"There was an error authorizing an add buddy request.", false},
		{GP_AUTHADD_BAD_FROM,"The profile being authorized is invalid. ", false},
		{GP_AUTHADD_BAD_SIG,"The signature for the authorization is invalid.", false},
		{GP_AUTHADD_IS_ON_BLOCKLIST,"The profile requesting authorization is on a block list.", false},
		//DOM-IGNORE-BEGIN 
		{GP_AUTHADD_IS_BLOCKING,"Reserved for future use.", false},
		//DOM-IGNORE-END

		// Error codes that can occur with status messages.
		{GP_STATUS,"There was an error with the status string.", false},

		// Error codes that can occur while sending a buddy message.
		{GP_BM,"There was an error sending a buddy message.", false},
		{GP_BM_NOT_BUDDY,"The profile the message was to be sent to is not a buddy.", false},
		{GP_BM_EXT_INFO_NOT_SUPPORTED,"The profile does not support extended info keys.", false},
		{GP_BM_BUDDY_OFFLINE,"The buddy to send a message to is offline.", false},

		// Error codes that can occur while getting profile information.
		{GP_GETPROFILE,"There was an error getting profile info. ", false},
		{GP_GETPROFILE_BAD_PROFILE,"The profile info was requested on is invalid.", false},

		// Error codes that can occur while deleting a buddy.
		{GP_DELBUDDY,"There was an error deleting the buddy.", false},
		{GP_DELBUDDY_NOT_BUDDY,"The buddy to be deleted is not a buddy. ", false},

		// Error codes that can occur while deleting your profile.
		{GP_DELPROFILE,"There was an error deleting the profile.", false},
		{GP_DELPROFILE_LAST_PROFILE,"The last profile cannot be deleted.", false},

		// Error codes that can occur while searching for a profile.
		{GP_SEARCH,"There was an error searching for a profile.", false},
		{GP_SEARCH_CONNECTION_FAILED,"The search attempt failed to connect to the server.", false},
		{GP_SEARCH_TIMED_OUT,"The search did not return in a timely fashion.", false},

		// Error codes that can occur while checking whether a user exists.
		{GP_CHECK,"There was an error checking the user account.", false},
		{GP_CHECK_BAD_EMAIL,"No account exists with the provided e-mail address.", false},
		{GP_CHECK_BAD_NICK,"No such profile exists for the provided e-mail address.", false},
		{GP_CHECK_BAD_PASSWORD,"The password is incorrect.", false},

		// Error codes that can occur while revoking buddy status.
		{GP_REVOKE,"There was an error revoking the buddy.", false},
		{GP_REVOKE_NOT_BUDDY,"You are not a buddy of the profile.", false},

		// Error codes that can occur while registering a new unique nick.
		{GP_REGISTERUNIQUENICK, "There was an error registering the uniquenick.", false},
		{GP_REGISTERUNIQUENICK_TAKEN, "The uniquenick is already taken.", false},
		{GP_REGISTERUNIQUENICK_RESERVED, "The uniquenick is reserved. ", false},
		{GP_REGISTERUNIQUENICK_BAD_NAMESPACE, "Tried to register a nick with no namespace set. ", false},

		// Error codes that can occur while registering a CD key.
		{GP_REGISTERCDKEY, "There was an error registering the cdkey.", false},
		{GP_REGISTERCDKEY_BAD_KEY, "The cdkey is invalid. ", false},
		{GP_REGISTERCDKEY_ALREADY_SET, "The profile has already been registered with a different cdkey.", false},
		{GP_REGISTERCDKEY_ALREADY_TAKEN, "The cdkey has already been registered to another profile. ", false},

		// Error codes that can occur while adding someone to your block list.
		{GP_ADDBLOCK, "There was an error adding the player to the blocked list. ", false},
		{GP_ADDBLOCK_ALREADY_BLOCKED, "The profile specified is already blocked.", false},

		// Error codes that can occur while removing someone from your block list.
		{GP_REMOVEBLOCK, "There was an error removing the player from the blocked list. ", false},
		{GP_REMOVEBLOCK_NOT_BLOCKED, "The profile specified was not a member of the blocked list.", false}

	};

	const GPErrorData getErrorDataByCode(GPErrorCode code) {
		GPErrorData ret;
		ret.error = (GPErrorCode) -1;
		ret.msg = NULL;
		ret.die = false;

		for(int i=0;i<sizeof(gp_error_data) / sizeof(GPErrorData);i++) {
			if(gp_error_data[i].error == code) {
				return gp_error_data[i];
			}
		}
		return ret;
	}
}