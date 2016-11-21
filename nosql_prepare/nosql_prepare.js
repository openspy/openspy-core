var redis = require("redis"),
    client = redis.createClient();

var mysql      = require('mysql');
var connection = mysql.createConnection({
  host     : 'localhost',
  user     : 'root',
  database : 'Gamemaster'
});

connection.connect();

client.on("error", function (err) {
    console.log("redis Error " + err);
});


connection.query('SELECT id, gamename, secretkey,description from games', function(err, rows, fields) {
  if (err) throw err;
  client.select(2);
  client.flushdb();
   for(var i = 0;i<rows.length;i++) {
		//console.log('The solution is: ' +  rows[i].id + ' ' +  rows[i].gamename + ' ' +  rows[i].secretkey + ' ' + rows[i].description);
		var gameid = rows[i].id,
			gamename = rows[i].gamename,
			secretkey = rows[i].secretkey,
			description = rows[i].description;
		var redis_key = gamename + ":" + gameid;
			
			client.hset(redis_key, "gameid", gameid);
			client.hset(redis_key, "gamename", gamename);
			client.hset(redis_key, "secretkey", secretkey);
			client.hset(redis_key, "description", description);
   }
 
});

connection.query('SELECT grp.gameid `gameid`, grp.groupid `groupid`,grp.name `name`,grp.numservers `numservers`,grp.numwaiting `numwaiting`,grp.password `pass`,g.gamename `gamename`, grp.maxwaiting `maxwaiting`, grp.other `other` from grouplist grp' +
		' INNER JOIN games g on g.id = grp.gameid', function(err, rows, fields) {
  if (err) throw err;
  client.select(1);
  client.flushdb();
   for(var i = 0;i<rows.length;i++) {
	var gamename = rows[i].gamename,
		pass = rows[i].pass,
		numwaiting = rows[i].numwaiting,
		numservers = rows[i].numservers,
		maxwaiting = rows[i].maxwaiting,
		name = rows[i].name,
		groupid = rows[i].groupid,
		gameid = rows[i].gameid,
		other = rows[i].other;
		
	var redis_key = gamename + ":" + groupid + ":";
	client.hset(redis_key, "gameid", gameid);
	client.hset(redis_key, "groupid", groupid);
	client.hset(redis_key, "maxwaiting", maxwaiting);	
	client.hset(redis_key, "hostname", name);	
	client.hset(redis_key, "password", pass);
	client.hset(redis_key, "numwaiting", numwaiting);
	client.hset(redis_key, "numservers", numservers);
	
	var res = other.split("\\");
	var key,value;
	for(var j = 1;j<res.length;j++) {
		var other_key = redis_key + "custkeys";
		if((j % 2) != 0) {
			key = res[j];
		} else {
			value = res[j];
			client.hset(other_key, key, value);
		}
	}
   }
});



client.unref();
connection.end();