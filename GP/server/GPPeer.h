#ifndef _GPPEER_H
#define _GPPEER_H
#include "../main.h"
#include <OS/Auth.h>
#include <OS/User.h>
#include <OS/Profile.h>

#define GPI_READ_SIZE                  (16 * 1024)
#define CHALLENGE_LEN 10

// Extended message support
#define GPI_NEW_AUTH_NOTIFICATION	(1<<0)
#define GPI_NEW_REVOKE_NOTIFICATION (1<<1)

// New Status Info support
#define GPI_NEW_STATUS_NOTIFICATION (1<<2)

// Buddy List + Block List retrieval on login
#define GPI_NEW_LIST_RETRIEVAL_ON_LOGIN (1<<3)

// Remote Auth logins now return namespaceid/partnerid on login
// so the input to gpInitialize is ignored
#define GPI_REMOTEAUTH_IDS_NOTIFICATION (1<<4)

// New CD Key registration style as opposed to using product ids
#define GPI_NEW_CDKEY_REGISTRATION (1<<5)

// New UDP Layer port
#define GPI_PEER_PORT 6500

#define GP_NICK_LEN                 31
#define GP_UNIQUENICK_LEN           21
#define GP_FIRSTNAME_LEN            31
#define GP_LASTNAME_LEN             31
#define GP_EMAIL_LEN                51
#define GP_PASSWORD_LEN             31
#define GP_PASSWORDENC_LEN          ((((GP_PASSWORD_LEN+2)*4)/3)+1)
#define GP_HOMEPAGE_LEN             76
#define GP_ZIPCODE_LEN              11
#define GP_COUNTRYCODE_LEN          3
#define GP_PLACE_LEN                128
#define GP_AIMNAME_LEN              51
#define GP_REASON_LEN               1025
#define GP_STATUS_STRING_LEN        256
#define GP_LOCATION_STRING_LEN      256
#define GP_ERROR_STRING_LEN         256
#define GP_AUTHTOKEN_LEN            256
#define GP_PARTNERCHALLENGE_LEN     256
#define GP_CDKEY_LEN                65
#define GP_CDKEYENC_LEN             ((((GP_CDKEY_LEN+2)*4)/3)+1)
#define GP_LOGIN_TICKET_LEN         25

#define GP_RICH_STATUS_LEN          256
#define GP_STATUS_BASIC_STR_LEN     33

/////////
// Types of bm's.
/////////////////
#define GPI_BM_MESSAGE                    1
#define GPI_BM_REQUEST                    2
#define GPI_BM_REPLY                      3  // only used on the backend
#define GPI_BM_AUTH                       4
#define GPI_BM_UTM                        5
#define GPI_BM_REVOKE                     6  // remote buddy removed from local list
#define GPI_BM_STATUS                   100						
#define GPI_BM_INVITE                   101
#define GPI_BM_PING                     102
#define GPI_BM_PONG                     103
#define GPI_BM_KEYS_REQUEST             104
#define GPI_BM_KEYS_REPLY               105
#define GPI_BM_FILE_SEND_REQUEST        200
#define GPI_BM_FILE_SEND_REPLY          201
#define GPI_BM_FILE_BEGIN               202
#define GPI_BM_FILE_END                 203
#define GPI_BM_FILE_DATA                204
#define GPI_BM_FILE_SKIP                205
#define GPI_BM_FILE_TRANSFER_THROTTLE   206
#define GPI_BM_FILE_TRANSFER_CANCEL     207
#define GPI_BM_FILE_TRANSFER_KEEPALIVE  208

//DEFINES
/////////
// Operation Types.
///////////////////
#define GPI_CONNECT                    0
#define GPI_NEW_PROFILE                1
#define GPI_GET_INFO                   2
#define GPI_PROFILE_SEARCH             3
#define GPI_REGISTER_UNIQUENICK        4
#define GPI_DELETE_PROFILE             5
#define GPI_REGISTER_CDKEY             6
// Operation States.
////////////////////
#define GPI_START                      0
#define GPI_CONNECTING               1
#define GPI_LOGIN                      2
#define GPI_REQUESTING                 3
#define GPI_WAITING                    4
#define GPI_FINISHING                  5


///////////////////////////////////////////////////////////////////////////////
// GPEnum
// Summary
//		Presence and Messaging SDK's general enum list. These are arguments
//		and return values for many GP functions.
typedef enum GPEnum
{
// Callback Types
GP_ERROR 				= 0, // Callback called whenever GP_NETWORK_ERROR or GP_SERVER_ERROR occur.
GP_RECV_BUDDY_REQUEST	= 1, // Callback called when another profile requests to add you to their buddy list.
GP_RECV_BUDDY_STATUS	= 2, // Callback called when one of your buddies changes status. 
GP_RECV_BUDDY_MESSAGE	= 3, // Callback called when someone has sent you a buddy message.
GP_RECV_BUDDY_UTM		= 4, // Callback called when someone has sent you a UTM message.
GP_RECV_GAME_INVITE		= 5, // Callback called when someone invites you to a game.
GP_TRANSFER_CALLBACK	= 6, // Callback called for status updates on a file transfer. 
GP_RECV_BUDDY_AUTH		= 7, // Callback called when someone authorizes your buddy request. 
GP_RECV_BUDDY_REVOKE	= 8, // Callback called when another profile stops being your buddy.

// Global states set with the gpEnable() function.
GP_INFO_CACHING 						= 256, // 0x100, Turns on full caching of profiles with gpEnable().
GP_SIMULATION							= 257, // 0x101, Turns on simulated GP function calls without 
											   // network traffic with gpEnable().
GP_INFO_CACHING_BUDDY_AND_BLOCK_ONLY	= 258, // 0x102, Recommended: Turns on caching of only buddy and 
											   // blocked list profiles with gpEnable().
#ifdef _PS3
GP_NP_SYNC                              = 259,  // 0x103,Turns on/off the NP to GP buddy sync   
#endif

// Blocking settings for certain GP functions, such as gpConnect().
GP_BLOCKING 	= 1, // Tells the function call to stop and wait for a callback.
GP_NON_BLOCKING = 0, // Recommended: Tells the function call to return and continue processing, 
					 // but gpProcess() must be called periodically.

// Firewall settings for the gpConnect() functions.
GP_FIREWALL		= 1, // Sets gpConnect() to send buddy messages through the GP backend.
GP_NO_FIREWALL	= 0, // Recommended: Sets gpConnect() to try to send buddy messages directly, 
					 // then fall back to the GP backend.

// Cache settings for the gpGetInfo() function.
GP_CHECK_CACHE		= 1, // Recommended: gpGetInfo() checks the local cache first for 
						 // profile data, then the GP backend.
GP_DONT_CHECK_CACHE = 0, // gpGetInfo() only queries the GP backend for profile data.

// Search result of gpIsValidEmail() given in GPIsValidEmailResponseArg.isValid.
GP_VALID	= 1, // Indicates in GPIsValidEmailResponseArg.isValid that a gpIsValidEmail() 
				 // call found the specified email address.
GP_INVALID	= 0, // Indicates in GPIsValidEmailResponseArg.isValid that a gpIsValidEmail() 
				 // call did NOT find the specified email address.

// Error severity indicator given in GPErrorArg.fatal.
GP_FATAL		= 1, // Indicates in GPErrorArg.fatal that a fatal GP_ERROR has occurred.
GP_NON_FATAL	= 0, // Indicates in GPErrorArg.fatal that a non-fatal GP_ERROR has occurred.

// Profile query result of gpGetInfo() given in GPGetInfoResponseArg.sex.
GP_MALE 	= 1280, // 0x500, Indicates in GPGetInfoResponseArg.sex that a 
					// gpGetInfo() call returned a male profile.
GP_FEMALE	= 1281, // 0x501, Indicates in GPGetInfoResponseArg.sex that a 
					// gpGetInfo() call returned a female profile.
GP_PAT		= 1282, // 0x502, Indicates in GPGetInfoResponseArg.sex that a 
					// gpGetInfo() call returned an sexless profile.

// Result of gpProfileSearch() given in GPProfileSearchResponseArg.more.
GP_MORE = 1536, // 0x600, Indicates in GPProfileSearchResponseArg.more that a 
				// gpProfileSearch() call has more matching records.
GP_DONE = 1537, // 0x601, Indicates in GPProfileSearchResponseArg.more that a 
				// gpProfileSearch() call has no more matching records.

// Profile fields used in gpGetInfo() and gpSetInfo() calls.
GP_NICK 			= 1792, // 0x700, Profile info used in gpGetInfo() and gpSetInfo() calls, length limit: 30.
GP_UNIQUENICK		= 1793, // 0x701, Profile info used in gpGetInfo() and gpSetInfo() calls, length limit: 20.
GP_EMAIL			= 1794, // 0x702, Profile info used in gpGetInfo() and gpSetInfo() calls, length limit: 50.
GP_PASSWORD			= 1795, // 0x703, Profile info used in gpGetInfo() and gpSetInfo() calls, length limit: 30.
GP_FIRSTNAME		= 1796, // 0x704, Profile info used in gpGetInfo() and gpSetInfo() calls, length limit: 30.
GP_LASTNAME			= 1797, // 0x705, Profile info used in gpGetInfo() and gpSetInfo() calls, length limit: 30.
GP_ICQUIN			= 1798, // 0x706, Profile info used in gpGetInfo() and gpSetInfo() calls.
GP_HOMEPAGE			= 1799, // 0x707, Profile info used in gpGetInfo() and gpSetInfo() calls, length limit: 75.
GP_ZIPCODE			= 1800, // 0x708, Profile info used in gpGetInfo() and gpSetInfo() calls, length limit: 10.
GP_COUNTRYCODE		= 1801, // 0x709, Profile info used in gpGetInfo() and gpSetInfo() calls, length limit:  2.
GP_BIRTHDAY			= 1802, // 0x70A, Profile info used in gpGetInfo() and gpSetInfo() calls.
GP_SEX				= 1803, // 0x70B, Profile info used in gpGetInfo() and gpSetInfo() calls.
GP_CPUBRANDID		= 1804, // 0x70C, Profile info used in gpGetInfo() and gpSetInfo() calls.
GP_CPUSPEED			= 1805, // 0x70D, Profile info used in gpGetInfo() and gpSetInfo() calls.
GP_MEMORY			= 1806, // 0x70E, Profile info used in gpGetInfo() and gpSetInfo() calls.
GP_VIDEOCARD1STRING	= 1807, // 0x70F, Profile info used in gpGetInfo() and gpSetInfo() calls.
GP_VIDEOCARD1RAM	= 1808, // 0x710, Profile info used in gpGetInfo() and gpSetInfo() calls.
GP_VIDEOCARD2STRING	= 1809, // 0x711, Profile info used in gpGetInfo() and gpSetInfo() calls.
GP_VIDEOCARD2RAM	= 1810, // 0x712, Profile info used in gpGetInfo() and gpSetInfo() calls.
GP_CONNECTIONID		= 1811, // 0x713, Profile info used in gpGetInfo() and gpSetInfo() calls.
GP_CONNECTIONSPEED	= 1812, // 0x714, Profile info used in gpGetInfo() and gpSetInfo() calls.
GP_HASNETWORK		= 1813, // 0x715, Profile info used in gpGetInfo() and gpSetInfo() calls.
GP_OSSTRING			= 1814, // 0x716, Profile info used in gpGetInfo() and gpSetInfo() calls.
GP_AIMNAME			= 1815, // 0x717, Profile info used in gpGetInfo() and gpSetInfo() calls, length limit: 50.
GP_PIC				= 1816, // 0x718, Profile info used in gpGetInfo() and gpSetInfo() calls.
GP_OCCUPATIONID		= 1817, // 0x719, Profile info used in gpGetInfo() and gpSetInfo() calls.
GP_INDUSTRYID		= 1818, // 0x71A, Profile info used in gpGetInfo() and gpSetInfo() calls.
GP_INCOMEID			= 1819, // 0x71B, Profile info used in gpGetInfo() and gpSetInfo() calls.
GP_MARRIEDID		= 1820, // 0x71C, Profile info used in gpGetInfo() and gpSetInfo() calls.
GP_CHILDCOUNT		= 1821, // 0x71D, Profile info used in gpGetInfo() and gpSetInfo() calls.
GP_INTERESTS1		= 1822, // 0x71E, Profile info used in gpGetInfo() and gpSetInfo() calls.

// gpNewProfile() overwrite settings.
GP_REPLACE		= 1, // Tells gpNewProfile() to overwrite a matching user profile.
GP_DONT_REPLACE = 0, // Recommended: Tells gpNewProfile() to notify you instead of 
					 // overwriting a matching user profile.

// Connection status indicators given by gpIsConnected().
GP_CONNECTED		= 1, // Output by gpIsConnected() when the GPConnection object 
						 // has a connection to the server.
GP_NOT_CONNECTED	= 0, // Output by gpIsConnected() when the GPConnection object 
						 // does NOT have a connection to the server.

// Bitwise OR-able field visibilities set in gpSetInfoMask() and returned by the gpGetInfo() callback.
GP_MASK_NONE        =  0, // 0x00, Indicates that none of the profile's fields are visible.
GP_MASK_HOMEPAGE    =  1, // 0x01, Indicates that the profile's homepage field is visible.
GP_MASK_ZIPCODE     =  2, // 0x02, Indicates that the profile's zipcode field is visible.
GP_MASK_COUNTRYCODE =  4, // 0x04, Indicates that the profile's country code field is visible.
GP_MASK_BIRTHDAY    =  8, // 0x08, Indicates that the profile's birthday field is visible.
GP_MASK_SEX         = 16, // 0x10, Indicates that the profile's sex field is visible.
GP_MASK_EMAIL       = 32, // 0x20, Indicates that the profile's email field is visible.
GP_MASK_BUDDYLIST	= 64, // 0x40, Indicates that the profile's buddy list is visible.
GP_MASK_ALL         = -1, // 0xFFFFFFFF, Indicates that all of a profile's fields are visible.

// Hidden buddy list indicator returned by the gpGetProfileBuddyList() 
// callback in GPGetProfileBuddyListArg.hidden.
GP_HIDDEN		= 1, // Indicates in GPGetProfileBuddyListArg.hidden that a 
					 // gpGetProfileBuddyList() call requested a profile that hides its buddies.
GP_NOT_HIDDEN	= 0, // Indicates in GPGetProfileBuddyListArg.hidden that a 
					 // gpGetProfileBuddyList() call requested a profile that does NOT hide its buddies.

// Buddy statuses given by a gpGetBuddyStatus() call in GPBuddyStatus.status.
GP_OFFLINE  = 0, // Indicates in GPBuddyStatus.status that a gpGetBuddyStatus() call 
				 // found a buddy that is not available.
GP_ONLINE   = 1, // Indicates in GPBuddyStatus.status that a gpGetBuddyStatus() call 
				 // found a buddy that is available.
GP_PLAYING  = 2, // Indicates in GPBuddyStatus.status that a gpGetBuddyStatus() call 
				 // found a buddy that is playing a game.
GP_STAGING  = 3, // Indicates in GPBuddyStatus.status that a gpGetBuddyStatus() call 
				 // found a buddy that is getting ready to play a game.
GP_CHATTING = 4, // Indicates in GPBuddyStatus.status that a gpGetBuddyStatus() call 
				 // found a buddy that is communicating.
GP_AWAY     = 5, // Indicates in GPBuddyStatus.status that a gpGetBuddyStatus() call 
				 // found a buddy that is not at his PC.

//DOM-IGNORE-BEGIN 
// Session types, reserved for future use.
GP_SESS_IS_CLOSED		=  1, // 0x01
GP_SESS_IS_OPEN			=  2, // 0x02
GP_SESS_HAS_PASSWORD	=  4, // 0x04
GP_SESS_IS_BEHIND_NAT	=  8, // 0x08
GP_SESS_IS_RANKED		= 16, // 0x10
//DOM-IGNORE-END

// CPU brands specified in GP_CPUBRANDID when calling gpSetInfoi().
GP_INTEL 	= 1, // Tells gpSetInfoi() the user's GP_CPUBRANDID is Intel.
GP_AMD		= 2, // Tells gpSetInfoi() the user's GP_CPUBRANDID is AMD.
GP_CYRIX	= 3, // Tells gpSetInfoi() the user's GP_CPUBRANDID is Cyrix.
GP_MOTOROLA	= 4, // Tells gpSetInfoi() the user's GP_CPUBRANDID is Motorola.
GP_ALPHA	= 5, // Tells gpSetInfoi() the user's GP_CPUBRANDID is Alpha.

// Internet connection types specified in GP_CONNECTIONID when calling gpSetInfoi().
GP_MODEM 		= 1, // Tells gpSetInfoi() the user's GP_CONNECTIONID is a modem.
GP_ISDN			= 2, // Tells gpSetInfoi() the user's GP_CONNECTIONID is ISDN.
GP_CABLEMODEM	= 3, // Tells gpSetInfoi() the user's GP_CONNECTIONID is a cable modem.
GP_DSL			= 4, // Tells gpSetInfoi() the user's GP_CONNECTIONID is DSL.
GP_SATELLITE	= 5, // Tells gpSetInfoi() the user's GP_CONNECTIONID is a satellite.
GP_ETHERNET		= 6, // Tells gpSetInfoi() the user's GP_CONNECTIONID is ethernet.
GP_WIRELESS		= 7, // Tells gpSetInfoi() the user's GP_CONNECTIONID is wireless.

// File transfer status update values received through GPTransferCallbackArg.type.
GP_TRANSFER_SEND_REQUEST 	= 2048, // 0x800, Indicates in GPTransferCallbackArg.type that a 
									// remote profile wants to send files to the local profile.
GP_TRANSFER_ACCEPTED		= 2049, // 0x801, Indicates in GPTransferCallbackArg.type that a 
									// transfer request has been accepted.
GP_TRANSFER_REJECTED		= 2050, // 0x802, Indicates in GPTransferCallbackArg.type that a 
									// transfer request has been rejected.
GP_TRANSFER_NOT_ACCEPTING	= 2051, // 0x803, Indicates in GPTransferCallbackArg.type that the 
									// remote profile is not accepting file transfers.
GP_TRANSFER_NO_CONNECTION	= 2052, // 0x804, Indicates in GPTransferCallbackArg.type that a 
									// direct connection with the remote profile could not be established.
GP_TRANSFER_DONE			= 2053, // 0x805, Indicates in GPTransferCallbackArg.type that the 
									// file transfer has finished successfully.
GP_TRANSFER_CANCELLED		= 2054, // 0x806, Indicates in GPTransferCallbackArg.type that the 
									// file transfer has been cancelled before completing.
GP_TRANSFER_LOST_CONNECTION	= 2055, // 0x807, Indicates in GPTransferCallbackArg.type that the 
									// direct connection with the remote profile has been lost.
GP_TRANSFER_ERROR			= 2056, // 0x808, Indicates in GPTransferCallbackArg.type that 
									// there was an error during the transfer.
GP_TRANSFER_THROTTLE		= 2057, // 0x809, Reserved for future use.
GP_FILE_BEGIN				= 2058, // 0x80A, Indicates in GPTransferCallbackArg.type that a 
									// file is about to be transferred.
GP_FILE_PROGRESS			= 2059, // 0x80B, Indicates in GPTransferCallbackArg.type that 
									// file data has been either sent or received.
GP_FILE_END					= 2060, // 0x80C, Indicates in GPTransferCallbackArg.type that a 
									// file has finished transferring successfully.
GP_FILE_DIRECTORY			= 2061, // 0x80D, Indicates in GPTransferCallbackArg.type that the 
									// current "file" being transferred is a directory name.
GP_FILE_SKIP				= 2062, // 0x80E, Indicates in GPTransferCallbackArg.type that the 
									// current file is being skipped.
GP_FILE_FAILED				= 2063, // 0x80F, Indicates in GPTransferCallbackArg.type that the 
									// current file being transferred has failed.

// File transfer error codes received through GPTransferCallbackArg.num.
GP_FILE_READ_ERROR	= 2304, // 0x900, Indicates in GPTransferCallbackArg.num that the 
							// sender had an error reading the file.
GP_FILE_WRITE_ERROR	= 2305, // 0x901, Indicates in GPTransferCallbackArg.num that the 
							// sender had an error writing the file.
GP_FILE_DATA_ERROR	= 2306, // 0x902, Indicates in GPTransferCallbackArg.num that the 
							// MD5 check of the data being transferred failed.

// File transfer direction indicator given by gpGetTransferSide().
GP_TRANSFER_SENDER 		= 2560, // 0xA00, Output by gpGetTransferSide() when the 
								// local profile is sending the file.
GP_TRANSFER_RECEIVER	= 2561, // 0xA01, Output by gpGetTransferSide() when the 
								// local profile is receiving the file.

// Flag for sending UTM messages directly (not backend routed) for gpSendBuddyUTM().
GP_DONT_ROUTE = 2816, // 0xB00, Tells gpSendBuddyUTM() to send this UTM message 
					  // directly to the buddy, instead of routing it through the backend.

// Bitwise OR-able quiet-mode flags used in the gpSetQuietMode() function.
GP_SILENCE_NONE       =  0, // Indicates to gpSetQuietMode() that no message types should be silenced.
GP_SILENCE_MESSAGES   =  1, // Indicates to gpSetQuietMode() that messages should be silenced.
GP_SILENCE_UTMS       =  2, // Indicates to gpSetQuietMode() that UTM type messages should be silenced.
GP_SILENCE_LIST       =  4, // Indicates to gpSetQuietMode() that list type messages should be silenced.
GP_SILENCE_ALL        = -1, // 0xFFFFFFFF, Indicates to gpSetQuietMode() that 
							// all message types should be silenced.

//DOM-IGNORE-BEGIN 
// New status info settings, reserved for future use.
GP_NEW_STATUS_INFO_SUPPORTED		= 3072, // 0xC00
GP_NEW_STATUS_INFO_NOT_SUPPORTED	= 3073  // 0xC01
//DOM-IGNORE-END

} GPEnum;

///////////////////////////////////////////////////////////////////////////////
// GPErrorCode
// Summary
//		Error codes which can occur in Presence and Messaging.
typedef enum GPErrorCode
{
// General error codes.
GP_GENERAL					= 0, // There was an unknown error. 
GP_PARSE					= 1, // Unexpected data was received from the server. 
GP_NOT_LOGGED_IN			= 2, // The request cannot be processed because user has not logged in. 
GP_BAD_SESSKEY				= 3, // The request cannot be processed because of an invalid session key.
GP_DATABASE					= 4, // There was a database error.
GP_NETWORK					= 5, // There was an error connecting a network socket.
GP_FORCED_DISCONNECT		= 6, // This profile has been disconnected by another login.
GP_CONNECTION_CLOSED		= 7, // The server has closed the connection.
GP_UDP_LAYER				= 8, // There was a problem with the UDP layer.

// Error codes that can occur while logging in.
GP_LOGIN						= 256, // 0x100, There was an error logging in to the GP backend.
GP_LOGIN_TIMEOUT				= 257, // 0x101, The login attempt timed out.
GP_LOGIN_BAD_NICK				= 258, // 0x102, The nickname provided was incorrect.
GP_LOGIN_BAD_EMAIL				= 259, // 0x103, The e-mail address provided was incorrect.
GP_LOGIN_BAD_PASSWORD			= 260, // 0x104, The password provided was incorrect.
GP_LOGIN_BAD_PROFILE			= 261, // 0x105, The profile provided was incorrect.
GP_LOGIN_PROFILE_DELETED		= 262, // 0x106, The profile has been deleted.
GP_LOGIN_CONNECTION_FAILED		= 263, // 0x107, The server has refused the connection.
GP_LOGIN_SERVER_AUTH_FAILED		= 264, // 0x108, The server could not be authenticated.
GP_LOGIN_BAD_UNIQUENICK			= 265, // 0x109, The uniquenick provided was incorrect.
GP_LOGIN_BAD_PREAUTH			= 266, // 0x10A, There was an error validating the pre-authentication.
GP_LOGIN_BAD_LOGIN_TICKET		= 267, // 0x10B, The login ticket was unable to be validated.
GP_LOGIN_EXPIRED_LOGIN_TICKET	= 268, // 0x10C, The login ticket had expired and could not be used.

// Error codes that can occur while creating a new user.
GP_NEWUSER						= 512, // 0x200, There was an error creating a new user.
GP_NEWUSER_BAD_NICK				= 513, // 0x201, A profile with that nick already exists.
GP_NEWUSER_BAD_PASSWORD			= 514, // 0x202, The password does not match the email address.
GP_NEWUSER_UNIQUENICK_INVALID	= 515, // 0x203, The uniquenick is invalid.
GP_NEWUSER_UNIQUENICK_INUSE		= 516, // 0x204, The uniquenick is already in use.

// Error codes that can occur while updating user information.
GP_UPDATEUI						= 768, // 0x300, There was an error updating the user information.
GP_UPDATEUI_BAD_EMAIL			= 769, // 0x301, A user with the email address provided already exists.

// Error codes that can occur while creating a new profile.
GP_NEWPROFILE					= 1024, // 0x400, There was an error creating a new profile.
GP_NEWPROFILE_BAD_NICK			= 1025, // 0x401, The nickname to be replaced does not exist.
GP_NEWPROFILE_BAD_OLD_NICK		= 1026, // 0x402, A profile with the nickname provided already exists.

// Error codes that can occur while updating profile information.
GP_UPDATEPRO					= 1280, // 0x500, There was an error updating the profile information. 
GP_UPDATEPRO_BAD_NICK			= 1281, // 0x501, A user with the nickname provided already exists.

// Error codes that can occur while adding someone to your buddy list.
GP_ADDBUDDY						= 1536, // 0x600, There was an error adding a buddy. 
GP_ADDBUDDY_BAD_FROM			= 1537, // 0x601, The profile requesting to add a buddy is invalid.
GP_ADDBUDDY_BAD_NEW				= 1538, // 0x602, The profile requested is invalid.
GP_ADDBUDDY_ALREADY_BUDDY		= 1539, // 0x603, The profile requested is already a buddy.
GP_ADDBUDDY_IS_ON_BLOCKLIST		= 1540, // 0x604, The profile requested is on the local profile's block list.
//DOM-IGNORE-BEGIN 
GP_ADDBUDDY_IS_BLOCKING			= 1541, // 0x605, Reserved for future use.
//DOM-IGNORE-END

// Error codes that can occur while being authorized to add someone to your buddy list.
GP_AUTHADD						= 1792, // 0x700, There was an error authorizing an add buddy request.
GP_AUTHADD_BAD_FROM				= 1793, // 0x701, The profile being authorized is invalid. 
GP_AUTHADD_BAD_SIG				= 1794, // 0x702, The signature for the authorization is invalid.
GP_AUTHADD_IS_ON_BLOCKLIST		= 1795, // 0x703, The profile requesting authorization is on a block list.
//DOM-IGNORE-BEGIN 
GP_AUTHADD_IS_BLOCKING			= 1796, // 0x704, Reserved for future use.
//DOM-IGNORE-END

// Error codes that can occur with status messages.
GP_STATUS						= 2048, // 0x800, There was an error with the status string.

// Error codes that can occur while sending a buddy message.
GP_BM							= 2304, // 0x900, There was an error sending a buddy message.
GP_BM_NOT_BUDDY					= 2305, // 0x901, The profile the message was to be sent to is not a buddy.
GP_BM_EXT_INFO_NOT_SUPPORTED	= 2306, // 0x902, The profile does not support extended info keys.
GP_BM_BUDDY_OFFLINE				= 2307, // 0x903, The buddy to send a message to is offline.

// Error codes that can occur while getting profile information.
GP_GETPROFILE					= 2560, // 0xA00, There was an error getting profile info. 
GP_GETPROFILE_BAD_PROFILE		= 2561, // 0xA01, The profile info was requested on is invalid.

// Error codes that can occur while deleting a buddy.
GP_DELBUDDY						= 2816, // 0xB00, There was an error deleting the buddy.
GP_DELBUDDY_NOT_BUDDY			= 2817, // 0xB01, The buddy to be deleted is not a buddy. 

// Error codes that can occur while deleting your profile.
GP_DELPROFILE					= 3072, // 0xC00, There was an error deleting the profile.
GP_DELPROFILE_LAST_PROFILE		= 3073, // 0xC01, The last profile cannot be deleted.

// Error codes that can occur while searching for a profile.
GP_SEARCH						= 3328, // 0xD00, There was an error searching for a profile.
GP_SEARCH_CONNECTION_FAILED		= 3329, // 0xD01, The search attempt failed to connect to the server.
GP_SEARCH_TIMED_OUT				= 3330, // 0XD02, The search did not return in a timely fashion.

// Error codes that can occur while checking whether a user exists.
GP_CHECK						= 3584, // 0xE00, There was an error checking the user account.
GP_CHECK_BAD_EMAIL				= 3585, // 0xE01, No account exists with the provided e-mail address.
GP_CHECK_BAD_NICK				= 3586,	// 0xE02, No such profile exists for the provided e-mail address.
GP_CHECK_BAD_PASSWORD			= 3587, // 0xE03, The password is incorrect.

// Error codes that can occur while revoking buddy status.
GP_REVOKE						= 3840, // 0xF00, There was an error revoking the buddy.
GP_REVOKE_NOT_BUDDY				= 3841, // 0xF01, You are not a buddy of the profile.

// Error codes that can occur while registering a new unique nick.
GP_REGISTERUNIQUENICK				= 4096, // 0x1000, There was an error registering the uniquenick.
GP_REGISTERUNIQUENICK_TAKEN			= 4097, // 0x1001, The uniquenick is already taken.
GP_REGISTERUNIQUENICK_RESERVED		= 4098, // 0x1002, The uniquenick is reserved. 
GP_REGISTERUNIQUENICK_BAD_NAMESPACE	= 4099, // 0x1003, Tried to register a nick with no namespace set. 

// Error codes that can occur while registering a CD key.
GP_REGISTERCDKEY				= 4352, // 0x1100, There was an error registering the cdkey.
GP_REGISTERCDKEY_BAD_KEY		= 4353, // 0x1101, The cdkey is invalid. 
GP_REGISTERCDKEY_ALREADY_SET	= 4354, // 0x1102, The profile has already been registered with a different cdkey.
GP_REGISTERCDKEY_ALREADY_TAKEN	= 4355, // 0x1103, The cdkey has already been registered to another profile. 

// Error codes that can occur while adding someone to your block list.
GP_ADDBLOCK						= 4608, // 0x1200, There was an error adding the player to the blocked list. 
GP_ADDBLOCK_ALREADY_BLOCKED		= 4609, // 0x1201, The profile specified is already blocked.

// Error codes that can occur while removing someone from your block list.
GP_REMOVEBLOCK					= 4864, // 0x1300, There was an error removing the player from the blocked list. 
GP_REMOVEBLOCK_NOT_BLOCKED		= 4865  // 0x1301, The profile specified was not a member of the blocked list.

} GPErrorCode;


typedef struct {
	GPEnum status; //GP_OFFLINE
	char status_str[GP_STATUS_STRING_LEN + 1];
	char location_str[GP_LOCATION_STRING_LEN + 1];
	OS::Address address;
	uint8_t quiet_flags;
} GPStatus;

typedef struct {
	GPErrorCode error;
	const char *msg;
	bool die;
} GPErrorData;

namespace GP {
	class Driver;

	class Peer {
	public:
		Peer(Driver *driver, struct sockaddr_in *address_info, int sd);
		~Peer();
		
		void think(bool packet_waiting);
		void handle_packet(char *data, int len);
		const struct sockaddr_in *getAddress() { return &m_address_info; }

		int GetProfileID();

		int GetSocket() { return m_sd; };

		bool ShouldDelete() { return m_delete_flag; };
		bool IsTimeout() { return m_timeout_flag; }

		int GetPing();
		void send_ping();

		void send_login_challenge(int type);
		void SendPacket(const uint8_t *buff, int len, bool attach_final = true);

		//event messages
		void send_add_buddy_request(int from_profileid, const char *reason);
		void send_authorize_add(int profileid);

		void inform_status_update(int profileid, GPStatus status);
		void send_revoke_message(int from_profileid, int date_unix_timestamp);
		//

	private:
		//packet handlers
		void handle_login(const char *data, int len);
		void handle_auth(const char *data, int len); //possibly for unexpected loss of connection to retain existing session

		void handle_status(const char *data, int len);
		void handle_statusinfo(const char *data, int len);

		void handle_addbuddy(const char *data, int len);
		void handle_delbuddy(const char *data, int len);
		void handle_revoke(const char *data, int len);
		void handle_authadd(const char *data, int len);

		void handle_pinvite(const char *data, int len);

		void handle_getprofile(const char *data, int len);
		int m_search_operation_id;
		static void m_getprofile_callback(bool success, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra);

		void handle_bm(const char *data, int len);

		void handle_addblock(const char *data, int len);
		void handle_removeblock(const char *data, int len);

		void handle_updatepro(const char *data, int len);
		void handle_updateui(const char *data, int len);

		void handle_keepalive(const char *data, int len);
		//

		//login
		void perform_nick_email_auth(const char *nick_email, int partnercode, const char *server_challenge, const char *client_challenge, const char *response);
		int m_auth_operation_id;
		static void m_nick_email_auth_cb(bool success, OS::User user, OS::Profile profile, OS::AuthData auth_data, void *extra);
		static void m_buddy_list_lookup_callback(bool success, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra);
		static void m_block_list_lookup_callback(bool success, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra);


		void send_buddies();
		void send_blocks();
		void send_error(GPErrorCode code);


		int m_sd;
		OS::GameData m_game;
		Driver *mp_driver;

		struct sockaddr_in m_address_info;

		struct timeval m_last_recv, m_last_ping;

		bool m_delete_flag;
		bool m_timeout_flag;

		char m_challenge[CHALLENGE_LEN + 1];

		GPStatus m_status;

		const char *mp_backend_session_key; //session key

		std::vector<int> m_buddies;

		static GPErrorData m_error_data[];

		OS::User m_user;
		OS::Profile m_profile;
	};
}
#endif //_GPPEER_H