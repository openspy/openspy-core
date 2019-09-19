create table users (
	id int auto_increment PRIMARY KEY,
	email varchar(51),
	password varchar(51),
	videocard1ram int null,
	videocard2ram int null,
	cpuspeed int null,
	cpubrandid int null,
	connectionspeed int null,
	hasnetwork tinyint(1) null,
	partnercode int not null default 0,
	publicmask int unsigned not null default 0,
	email_verified tinyint(1) not null default 0,
	deleted tinyint(1) not null default 0
);

create table profiles (
	id int auto_increment PRIMARY KEY,
	nick varchar(31) null,
	uniquenick varchar(21) null,
	firstname  varchar(31) null,
	lastname  varchar(31) null,
	homepage  text null,
	icquin int null,
	zipcode int null,
	sex int null,
	birthday date null,
	ooc int null,
	ind int null,
	inc int null,
	i1 int null,
	o1 int null,
	mar int null,
	chc int null,
	conn int null,
	lon decimal null,
	lat decimal null,
	aimname text null,
	countrycode text null,
	pic int null,
	userid int not null,
	namespaceid int not null default 0,
	deleted boolean not null default false,
	admin boolean not null default false,
	foreign key fk_user(userid)
	references users(id)
);

create table buddies (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	to_profileid int,
	from_profileid int,
	foreign key fk_buddies_to_profileid(to_profileid)
	references profiles(id),
	foreign key fk_buddies_from_profileid(from_profileid)
	references profiles(id),
  PRIMARY KEY (`id`)
);

create table blocks (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	to_profileid int,
	from_profileid int,
	foreign key fk_blocks_to_profileid(to_profileid)
	references profiles(id),
	foreign key fk_blocks_from_profileid(from_profileid)
	references profiles(id),
  PRIMARY KEY (`id`)
);


CREATE TABLE `persist_data` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `modified` timestamp NULL DEFAULT NULL,
  `base64Data` blob,
  `persist_type` int(11) DEFAULT NULL,
  `data_index` int(11) DEFAULT NULL,
  `profileid` int(11) DEFAULT NULL,
  `gameid` int(11) NOT NULL,
  foreign key fk_pd_profile(profileid)
  references profiles(id),
  PRIMARY KEY (`id`)
);



CREATE TABLE `persist_keyed_data` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `key_name` text,
  `key_value` blob,
  `profileid` int(11) DEFAULT NULL,
  `gameid` int(11) DEFAULT NULL,
  `persist_type` int(11) DEFAULT NULL,
  `data_index` int(11) DEFAULT NULL,
  `modified` datetime DEFAULT NULL,
  foreign key fk_pkd_profile(profileid)
  references profiles(id),
  PRIMARY KEY (`id`)
)