use Peerchat;

create table usermodes ( 
	id int auto_increment PRIMARY KEY,
	channelmask text NOT NULL,
	hostmask text NULL,
    comment text NULL,
    machineid text NULL,
    profileid int NULL,
    modeflags int NOT NULL DEFAULT 0,
    expiresAt DATETIME NULL,
    ircNick text NULL,
    setByHost text NULL,
    setByPid int NULL,
    setAt DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
);