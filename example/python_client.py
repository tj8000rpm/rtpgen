#!/usr/bin/python -i
# -*- encoding=utf-8 -*-
import sys
import socket
import ipc_pack_pb2 as pb
import time
import functools

s=None

def ip_i2s(ip_addr):
  return "{}.{}.{}.{}".format((ip_addr>>24)&0xff,
                              (ip_addr>>16)&0xff,
                              (ip_addr>> 8)&0xff,
                              (ip_addr>> 0)&0xff)
def ip_s2i(ip_addr_str):
  try:
    h1,h2,h3,h4=ip_addr_str.split('.',4)
    h1=int(h1)
    h2=int(h2)
    h3=int(h3)
    h4=int(h4)
    return (h1<<24)+(h2<<16)+(h3<<8)+h4
  except:
    return None;

def printVal(res):
  print('-'*30)
  if res.HasField("request_code"):
    print('Request code: {}'.format(res.request_code))
  if res.HasField("response_code"):
    print('Response code: {}'.format(res.response_code))
  if res.HasField("id_selector"):
    print('id_selector: {}'.format(res.id_selector))
  if res.HasField("portid"):
    print('portid: {}'.format(res.portid))
  conf=res.rtp_config
  if conf.HasField("ip_dst_addr") and conf.HasField("udp_dst_port"):
    print('dst: {}:{}'.format(ip_i2s(conf.ip_dst_addr),conf.udp_dst_port))
  elif conf.HasField("ip_dst_addr") and not conf.HasField("udp_dst_port"):
    print('dst: {}'.format(ip_i2s(conf.ip_dst_addr)))
  elif not conf.HasField("ip_dst_addr") and conf.HasField("udp_dst_port"):
    print('dst: ?:{}'.format(conf.udp_dst_port))
  if conf.HasField("ip_src_addr") and conf.HasField("udp_src_port"):
    print('src: {}:{}'.format(ip_i2s(conf.ip_src_addr),conf.udp_src_port))
  elif conf.HasField("ip_src_addr") and not conf.HasField("udp_src_port"):
    print('src: {}'.format(ip_i2s(conf.ip_src_addr)))
  elif not conf.HasField("ip_src_addr") and conf.HasField("udp_src_port"):
    print('src: ?:{}'.format(conf.udp_src_port))

  if conf.HasField("rtp_timestamp"):
    print('timestamp: {}'.format(conf.rtp_timestamp))
  if conf.HasField("rtp_sequence"):
    print('sequence : {}'.format(conf.rtp_sequence))
  if conf.HasField("rtp_ssrc"):
    print('ssrc     : {}'.format(conf.rtp_ssrc))

def getrsc(portid,selector):
  pack=pb.RtpgenIPCmsgV1()
  pack.request_code=pb.RtpgenIPCmsgV1.READ
  pack.id_selector=selector
  pack.portid=portid
  
  s.send(pack.SerializeToString())
  
  data=s.recv(1024)
  
  res=pb.RtpgenIPCmsgV1()
  res.ParseFromString(data)
  
  if not res.HasField("response_code"):
    print('response error.')
  return res

def delrsc(portid,selector):
  pack=pb.RtpgenIPCmsgV1()
  pack.request_code=pb.RtpgenIPCmsgV1.DELETE
  pack.id_selector=selector
  pack.portid=portid
  
  s.send(pack.SerializeToString())
  
  data=s.recv(1024)
  
  res=pb.RtpgenIPCmsgV1()
  res.ParseFromString(data)
  
  if not res.HasField("response_code"):
    print('response error.')
  return res

def crersc(portid,selector,src=None,dst=None,timestmap=None,sequence=None,ssrc=None):
  return writersc(pb.RtpgenIPCmsgV1.CREATE, portid,selector,src,dst,timestmap,sequence,ssrc)

def putrsc(portid,selector,src=None,dst=None,timestmap=None,sequence=None,ssrc=None):
  return writersc(pb.RtpgenIPCmsgV1.UPDATE, portid,selector,src,dst,timestmap,sequence,ssrc)

def writersc(request_code,portid,selector,src=None,dst=None,timestmap=None,sequence=None,ssrc=None):
  pack=pb.RtpgenIPCmsgV1()
  pack.request_code=request_code
  pack.id_selector=selector
  pack.portid=portid

  if(src!=None):
      ip, port=src;
      pack.rtp_config.ip_src_addr=ip_s2i(ip)
      pack.rtp_config.udp_src_port=port
  if(dst!=None):
      ip, port=dst;
      pack.rtp_config.ip_dst_addr=ip_s2i(ip)
      pack.rtp_config.udp_dst_port=port
  if(timestmap!=None):
      pack.rtp_config.rtp_timestmap=timestmap
  if(sequence!=None):
      pack.rtp_config.rtp_sequence=sequence
  if(ssrc!=None):
      pack.rtp_config.rtp_ssrc=ssrc
  
  s.send(pack.SerializeToString())
  
  data=s.recv(1024)
  
  res=pb.RtpgenIPCmsgV1()
  res.ParseFromString(data)
  
  if not res.HasField("response_code"):
    print('response error.')
  return res

def get(*args):
  printVal(getrsc(*args))

def cre(*args, **kargs):
  printVal(crersc(*args,**kargs))

def put(*args, **kargs):
  printVal(putrsc(*args,**kargs))

def rmv(*args):
  printVal(delrsc(*args))

def end():
  try:
    s.close()
  except:
    pass
  sys.exit(0)

def help():
  print('''\
+-------------------------------------------------------------+
| Help                                                        |
+-------------------------------------------------------------+
| # create new RTP stream                                     |
| >> cre(PORTID, SESSION_ID,                                  |
|        src=('192.168.0.3',8000), dst=('192.168.0.3',6000) ) |
|                                                             |
| # get status existing RTP stream                            |
| >> get(PORTID, SESSION_ID)                                  |
|                                                             |
| # update existing RTP stream                                |
| >> put(PORTID, SESSION_ID,                                  |
|        src=('192.168.0.3',8000), dst=('192.168.0.3',6000) ) |
|                                                             |
| # delete existing RTP stream                                |
| >> rmv(PORTID, SESSION_ID)                                  |
|                                                             |
| # client disconnecting srever and exit cli                  |
| >> quit()                                                   |
+-------------------------------------------------------------+
''')                                                             

s=socket.socket(socket.AF_INET,socket.SOCK_STREAM)
s.connect(('localhost',55077))

help()

## run as interactivemode
