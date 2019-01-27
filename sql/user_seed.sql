delete from buddies where id > -1;
delete from profiles where id > -1;
delete from users where id > -1;

insert into users (id,email,password) VALUES 
    (1,"gptestc@gptestc.com", "gptestc"),
    (2,"gptestc2@gptestc.com", "gptestc"),
    (3,"gptestc3@gptestc.com", "gptestc"),
    (4, "dan@gamespy.com", "dan123"),
    (5, "crt@gamespy.com", "crt123")


insert into profiles (id, nick, uniquenick, namespaceid, userid, firstname, lastname) values 
(2957553, "gptestc1", "gptestc1", 0, 1, "firstname", "lastname"),
 -- (2957554, "gptestc1", "gptestc1", 1, 1, "firstname", "lastname"),
(3052160, "gptestc2", "gptestc2", 0, 2, "firstname", "lastname"),
(118305038, "gptestc3", "gptestc3", 0, 3, "firstname", "lastname"),
(1, "crt", "crt", 0, 5, "firstname", "lastname"),
(2, "dan", "dan", 0, 4, "firstname", "lastname")


