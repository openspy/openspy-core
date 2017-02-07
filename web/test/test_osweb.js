var chakram = require('chakram'),
expect = chakram.expect;

var gateway_address = "http://10.10.10.10";

describe("Base Tests", function() {
    it("test gateway up", function() {
      var resp = chakram.post(gateway_address);
      expect(resp).to.have.status(400);
      return chakram.wait();
  });
});

var login_details = {uniquenick: "sctest01", password: "gspy", email: "sctest@gamespy.com", namespaceid: 0, partnercode: 0, nick: "sctest01"};
var user_schema = {"required": [
              "email",
              "email_verified",
              "videocard1ram",
              "videocard2ram",
              "cpuspeed",
              "cpubrandid",
              "connectionspeed",
              "hasnetwork",              
              "publicmask",
              "partnercode",
              "deleted"
            ]
          };


describe("Registration Service", function() {

    var resp;

    before(function () {
      var post_params = {};
      post_params.uniquenick = login_details.uniquenick;
      post_params.nick = "sctest01";
      post_params.password = "gspy";
      post_params.email = "sctest@gamespy.com";
      post_params.namespaceid = 0;
      post_params.partnercode = 0;

      post_params.mode = "create_account";

      resp = chakram.post(gateway_address + "/web/registersvc", post_params);
      chakram.wait();
      return resp;
    });

    it("response code", function() {
      return expect(resp).to.have.status(200);       
    });


    it("response success", function() {
      return expect(resp).to.have.json("success", true);
    });

    it("has valid user", function() {
      return expect(resp).to.have.schema("user", 
          user_schema
        );
    });
});

describe("Auth Service", function() {
    var resp;
    var session_key;
    var user_id;
    before(function () {
      var post_params = {};
      post_params.mode = "auth";
      post_params.email = login_details.email;
      post_params.nick = login_details.nick;
      post_params.uniquenick = login_details.uniquenick;
      post_params.password = login_details.password;
      post_params.namespaceid = login_details.namespaceid;
      post_params.partnercode = login_details.partnercode;


      resp = chakram.post(gateway_address + "/web/auth", post_params);
      chakram.wait();
      return resp;
    });

    
    it("response code", function() {
      return expect(resp).to.have.status(200);       
    });

    it("got session key", function() {
      return expect(resp).to.have.json('session_key', function (url) {
          expect(url).to.not.equal(undefined);
          session_key = url;
      });

    });

    it("test got userid", function() {
      expect(resp).to.have.json('profile', function (url) {
          expect(url).to.not.equal(undefined);
      });
      
      expect(resp).to.have.json('profile.id', function(id) {
        expect(id).to.not.equal(undefined);
        user_id = id;
      });
    });

    it("test session valid", function() {
      var post_params = {};
      post_params.session_key = session_key;
      post_params.userid = user_id;
      post_params.mode = "test_session";


      chakram.post(gateway_address + "/web/auth", post_params).then(function(result) {
        try {
          expect(result).to.have.json('valid', true);
          done();
        } catch (e) {
          done(e);
        }
      });

    });

    it("test delete session", function() {
      var post_params = {};
      post_params.session_key = session_key;
      post_params.userid = user_id;
      post_params.mode = "del_session";


      chakram.post(gateway_address + "/web/auth", post_params).then(function(result) {
        try {
          expect(result).to.have.json('valid', true);
          done();
        } catch (e) {
          done(e);
        }
      });

    });

    it("test session deleted", function() {
      var post_params = {};
      post_params.session_key = session_key;
      post_params.userid = user_id;
      post_params.mode = "test_session";


      chakram.post(gateway_address + "/web/auth", post_params).then(function(result) {
        try {
          expect(result).to.have.json('valid', false);
          done();
        } catch (e) {
          done(e);
        }
      });

    });
    return chakram.wait();
});