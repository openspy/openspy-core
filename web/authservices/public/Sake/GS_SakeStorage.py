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
        resp_xml = ET.Element('SOAP-ENV:Envelope')
        body = ET.SubElement(resp_xml, 'SOAP-ENV:Body')
        login_result = ET.SubElement(body, 'ns1:GetRandomRecordsResponse')

        result_node = ET.SubElement(login_result, 'ns1:GetRandomRecordsResult')
        result_node.text = 'Success'

       
        #auth stuff

        numratings_node = ET.SubElement(login_result, 'ns1:values')

        #avgrating_node = ET.SubElement(numratings_node, 'ns1:ArrayOfRecordValue')
        

        return resp_xml
    def handle_get_record_limit(self, xml_tree):
        resp_xml = ET.Element('SOAP-ENV:Envelope')
        body = ET.SubElement(resp_xml, 'SOAP-ENV:Body')
        login_result = ET.SubElement(body, 'ns1:GetRecordLimitResponse')

        result_node = ET.SubElement(login_result, 'ns1:GetRecordLimitResult')
        result_node.text = 'Success'
       
        #auth stuff

        numratings_node = ET.SubElement(login_result, 'ns1:limitPerOwner')
        numratings_node.text = '0'

        avgrating_node = ET.SubElement(login_result, 'ns1:numOwned')
        avgrating_node.text = '0'        

        return resp_xml
    def handle_create_record(self, xml_tree):
        resp_xml = ET.Element('SOAP-ENV:Envelope')
        body = ET.SubElement(resp_xml, 'SOAP-ENV:Body')
        login_result = ET.SubElement(body, 'ns1:CreateRecordResponse')

        result_node = ET.SubElement(login_result, 'ns1:CreateRecordResult')
        result_node.text = 'Success'

       
        #auth stuff

        recordid_node = ET.SubElement(login_result, 'ns1:recordid')
        recordid_node.text = '0'
        return resp_xml
    def handle_update_record(self, xml_tree):
        resp_xml = ET.Element('SOAP-ENV:Envelope')
        body = ET.SubElement(resp_xml, 'SOAP-ENV:Body')
        login_result = ET.SubElement(body, 'ns1:UpdateRecordResponse')

        result_node = ET.SubElement(login_result, 'ns1:UpdateRecordResult')
        result_node.text = 'Success'

       
        #auth stuff

        recordid_node = ET.SubElement(login_result, 'ns1:recordid')
        recordid_node.text = '0'
    

        return resp_xml
    def handle_delete_record(self, xml_tree):
        resp_xml = ET.Element('SOAP-ENV:Envelope')
        body = ET.SubElement(resp_xml, 'SOAP-ENV:Body')
        login_result = ET.SubElement(body, 'ns1:DeleteRecordResponse')

        result_node = ET.SubElement(login_result, 'ns1:DeleteRecordResult')
        result_node.text = 'Success'

       
        #auth stuff

        recordid_node = ET.SubElement(login_result, 'ns1:recordid')
        recordid_node.text = '0'
    

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

        namespaces = {"SOAP-ENV": "http://schemas.xmlsoap.org/soap/envelope/", "SOAP-ENC":"http://schemas.xmlsoap.org/soap/encoding/", "xsi": "http://www.w3.org/2001/XMLSchema-instance",
        "xsd":"http://www.w3.org/2001/XMLSchema",
        "ns1": "http://gamespy.net/sake"}

        tree = ET.ElementTree(ET.fromstring(request_body))

        names = tree.findall('SOAP-ENV:Body', namespaces)
        body = names[0]

        for request in body.getchildren():
            tag = request.tag
            print("Tag: {}\n".format(tag))
            request_info = self.validate_request(request)
            #print("Info: {}\n".format(request_info))
            if not request_info["success"]:
                return b'Validation error'
            self.dump_xml_records(request)            
            if '}' in tag:
                tag = tag.split('}', 1)[1]  # strip all namespaces
            #if tag == "RateRecord":
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
            #if tag == "GetRecordLimit":
            #if tag == "GetSpecificRecords":
            #if tag == "GetMyRecords":
            #if tag == "SearchForRecords":
            #if tag == "UpdateRecord":
            #if tag == "DeleteRecord":
            #if tag == "CreateRecord":


        if resp != None:
            ret_str = ET.tostring(resp, encoding='utf8', method='xml')
            print("Ret: {}\n".format(ret_str))
            return ret_str
        else:
            return b'Fatal error'
