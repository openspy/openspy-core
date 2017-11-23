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
	nick varchar(31) not null,
	uniquenick varchar(21) null,
	firstname  varchar(31) null,
	lastname  varchar(31) null,
	homepage  text null,
	icquin int null,
	zipcode int null,
	sex int null,
	birthday int null,
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
	foreign key fk_user(userid)
	references users(id)
);

create table buddies (
	to_profileid int,
	from_profileid int,
	foreign key fk_buddies_to_profileid(to_profileid)
	references profiles(id),
	foreign key fk_buddies_from_profileid(from_profileid)
	references profiles(id)
);

create table blocks (
	to_profileid int,
	from_profileid int,
	foreign key fk_blocks_to_profileid(to_profileid)
	references profiles(id),
	foreign key fk_blocks_from_profileid(from_profileid)
	references profiles(id)
);