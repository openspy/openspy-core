from cgi import parse_qs, escape
import xml.etree.ElementTree as ET

import binascii
import hashlib

from collections import OrderedDict

import simplejson as json
import http.client
import struct, os
import rsa

from Model.Game import Game
from Model.Profile import Profile

from BaseService import BaseService

class GS_SakeStorageSvc(BaseService):
    def dump_xml_records(self, xml_tree):
        for request in xml_tree.getchildren():
            tag = request.tag
            if '}' in tag:
                tag = tag.split('}', 1)[1]  # strip all namespaces
            #print("Child: {}\n".format(tag))
    def handle_get_random_records(self, xml_tree):
        resp_xml = ET.Element('{http://schemas.xmlsoap.org/soap/envelope/}Envelope', self.namespaces)
        body = ET.SubElement(resp_xml, '{http://schemas.xmlsoap.org/soap/envelope/}Body')
        login_result = ET.SubElement(body, '{http://gamespy.net/sake}GetRandomRecordsResponse')

        result_node = ET.SubElement(login_result, '{http://gamespy.net/sake}GetRandomRecordsResult')
        result_node.text = 'Success'

        values_node = ET.SubElement(login_result, '{http://gamespy.net/sake}values')

        #avgrating_node = ET.SubElement(numratings_node, '{http://gamespy.net/sake}ArrayOfRecordValue')
        

        return resp_xml
    def handle_get_record_limit(self, xml_tree):
        resp_xml = ET.Element('{http://schemas.xmlsoap.org/soap/envelope/}Envelope')
        body = ET.SubElement(resp_xml, '{http://schemas.xmlsoap.org/soap/envelope/}Body')
        login_result = ET.SubElement(body, '{http://gamespy.net/sake}GetRecordLimitResponse')

        result_node = ET.SubElement(login_result, '{http://gamespy.net/sake}GetRecordLimitResult')
        result_node.text = 'Success'
       
        #auth stuff

        numratings_node = ET.SubElement(login_result, '{http://gamespy.net/sake}limitPerOwner')
        numratings_node.text = '0'

        avgrating_node = ET.SubElement(login_result, '{http://gamespy.net/sake}numOwned')
        avgrating_node.text = '0'        

        return resp_xml
    def handle_create_record(self, xml_tree):
        resp_xml = ET.Element('{http://schemas.xmlsoap.org/soap/envelope/}Envelope')
        body = ET.SubElement(resp_xml, '{http://schemas.xmlsoap.org/soap/envelope/}Body')
        login_result = ET.SubElement(body, '{http://gamespy.net/sake}CreateRecordResponse')

        result_node = ET.SubElement(login_result, '{http://gamespy.net/sake}CreateRecordResult')
        result_node.text = 'Success'

        tableid_node = ET.SubElement(login_result, '{http://gamespy.net/sake}recordid')
        tableid_node.text = '0'

        return resp_xml
    def handle_update_record(self, xml_tree):
        resp_xml = ET.Element('{http://schemas.xmlsoap.org/soap/envelope/}Envelope')
        body = ET.SubElement(resp_xml, '{http://schemas.xmlsoap.org/soap/envelope/}Body')
        login_result = ET.SubElement(body, '{http://gamespy.net/sake}UpdateRecordResponse')

        result_node = ET.SubElement(login_result, '{http://gamespy.net/sake}UpdateRecordResult')
        result_node.text = 'Success'

        tableid_node = ET.SubElement(login_result, '{http://gamespy.net/sake}recordid')
        tableid_node.text = '0'
        return resp_xml
    def handle_delete_record(self, xml_tree):
        resp_xml = ET.Element('{http://schemas.xmlsoap.org/soap/envelope/}Envelope')
        body = ET.SubElement(resp_xml, '{http://schemas.xmlsoap.org/soap/envelope/}Body')
        login_result = ET.SubElement(body, '{http://gamespy.net/sake}DeleteRecordResponse')

        result_node = ET.SubElement(login_result, '{http://gamespy.net/sake}DeleteRecordResult')
        result_node.text = 'Success'

        return resp_xml
    def handle_get_specific_records(self, xml_tree):
        resp_xml = ET.Element('{http://schemas.xmlsoap.org/soap/envelope/}Envelope')
        body = ET.SubElement(resp_xml, '{http://schemas.xmlsoap.org/soap/envelope/}Body')
        login_result = ET.SubElement(body, '{http://gamespy.net/sake}GetSpecificRecordsResponse')

        result_node = ET.SubElement(login_result, '{http://gamespy.net/sake}GetSpecificRecordsResult')
        result_node.text = 'Success'

        values_node = ET.SubElement(login_result, '{http://gamespy.net/sake}values')

        return resp_xml
    def handle_rate_records(self, xml_tree):
        resp_xml = ET.Element('{http://schemas.xmlsoap.org/soap/envelope/}Envelope')
        body = ET.SubElement(resp_xml, '{http://schemas.xmlsoap.org/soap/envelope/}Body')
        login_result = ET.SubElement(body, '{http://gamespy.net/sake}RateRecordResponse')

        result_node = ET.SubElement(login_result, '{http://gamespy.net/sake}RateRecordResult')
        result_node.text = 'Success'

        result_node = ET.SubElement(login_result, '{http://gamespy.net/sake}numRatings')
        result_node.text = '0'

        result_node = ET.SubElement(login_result, '{http://gamespy.net/sake}averageRating')
        result_node.text = '0'

        return resp_xml
    def handle_get_my_records(self, xml_tree):
        resp_xml = ET.Element('{http://schemas.xmlsoap.org/soap/envelope/}Envelope')
        body = ET.SubElement(resp_xml, '{http://schemas.xmlsoap.org/soap/envelope/}Body')
        login_result = ET.SubElement(body, '{http://gamespy.net/sake}GetMyRecordsResponse')

        result_node = ET.SubElement(login_result, '{http://gamespy.net/sake}GetMyRecordsResult')
        result_node.text = 'Success'

        values_node = ET.SubElement(login_result, '{http://gamespy.net/sake}values')
        return resp_xml
    def handle_search_for_records(self, xml_tree):
        resp_xml = ET.Element('{http://schemas.xmlsoap.org/soap/envelope/}Envelope')
        body = ET.SubElement(resp_xml, '{http://schemas.xmlsoap.org/soap/envelope/}Body')
        login_result = ET.SubElement(body, '{http://gamespy.net/sake}SearchForRecordsResponse')

        result_node = ET.SubElement(login_result, '{http://gamespy.net/sake}SearchForRecordsResult')
        result_node.text = 'Success'

        values_node = ET.SubElement(login_result, '{http://gamespy.net/sake}values')
        return resp_xml
    def validate_request(self, xml_tree):
        request_info = {"success": False}
        game_id = xml_tree.find("{http://gamespy.net/sake}gameid").text
        secretkey = xml_tree.find("{http://gamespy.net/sake}secretKey").text
        loginTicket = xml_tree.find("{http://gamespy.net/sake}loginTicket").text
        #gameid = xml_tree.find()
        #print("GameID: {}\n".format(game_id))
        game = Game.select().where(Game.id == game_id).get()
        profile = Profile.select().where(Profile.id == 10000).get()
        #print("Profile: {}\nGame: {}\n{} - {}\n".format(profile,game, game.secretkey, secretkey))
        if game.secretkey != secretkey:
            return request_info

        request_info["success"] = True
        request_info["game"] = game
        request_info["profile"] = profile
        return request_info
    def run(self, env, start_response):
        resp = None
        # the environment variable CONTENT_LENGTH may be empty or missing
        try:
            request_body_size = int(env.get('CONTENT_LENGTH', 0))
        except (ValueError):
            request_body_size = 0

        # When the method is POST the variable will be sent
        # in the HTTP request body which is passed by the WSGI server
        # in the file like wsgi.input environment variable.
        request_body = env['wsgi.input'].read(request_body_size)
       # d = parse_qs(request_body)
        start_response('200 OK', [('Content-Type','application/xml')])

        self.namespaces = {"SOAP-ENV": "http://schemas.xmlsoap.org/soap/envelope/", "SOAP-ENC":"http://schemas.xmlsoap.org/soap/encoding/", "xsi": "http://www.w3.org/2001/XMLSchema-instance",
        "xsd":"http://www.w3.org/2001/XMLSchema"}
        self.custom_namespaces = {"ns1": "http://gamespy.net/sake"}

        for key in self.namespaces:
            ET.register_namespace(key,self.namespaces[key])

        tree = ET.ElementTree(ET.fromstring(request_body))

        names = tree.findall('SOAP-ENV:Body', {**self.custom_namespaces, **self.namespaces})
        body = names[0]

        for request in body.getchildren():
            tag = request.tag
            #print("Tag: {}\n".format(tag))
            request_info = self.validate_request(request)
            #print("Info: {}\n".format(request_info))
            if not request_info["success"]:
                return b'Validation error'
            self.dump_xml_records(request)            
            if '}' in tag:
                tag = tag.split('}', 1)[1]  # strip all namespaces
            if tag == "GetRandomRecords":
                resp = self.handle_get_random_records(request)
            elif tag == "GetRecordLimit":
                resp = self.handle_get_record_limit(request)
            elif tag == "CreateRecord":
                resp = self.handle_create_record(request)
            elif tag == "UpdateRecord":
                resp = self.handle_update_record(request)
            elif tag == "DeleteRecord":
                resp = self.handle_delete_record(request)
            elif tag == "GetSpecificRecords":
                resp = self.handle_get_specific_records(request)
            elif tag == "RateRecord":
                resp = self.handle_rate_records(request)
            elif tag == "GetMyRecords":
                resp = self.handle_get_my_records(request)
            elif tag == "SearchForRecords":
                resp = self.handle_search_for_records(request)
            else:
                print("Unknown tag: {}\n".format(tag))


        if resp != None:
            #print("Req: {}\n".format(request_body))
            ret_str = ET.tostring(resp, encoding='utf8', method='xml')
            #print("Ret: {}\n".format(ret_str))
            return ret_str
        else:
            return b'Fatal error'
