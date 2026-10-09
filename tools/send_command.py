#!/usr/bin/env python3
"""Publish a non-retained QoS1 command; credentials come from environment."""
import argparse
import json
import os
import ssl
import threading
from pathlib import Path
import paho.mqtt.client as mqtt

def main():
    p=argparse.ArgumentParser();p.add_argument('command',type=Path);p.add_argument('--gateway',required=True);a=p.parse_args()
    text=a.command.read_text();j=json.loads(text)
    if not j.get('job_id'):p.error('job_id required')
    if len(text.encode())>1023:p.error('command exceeds gateway buffer (1023 bytes)')
    host=os.environ['MQTT_HOST'];port=int(os.getenv('MQTT_PORT','8883'))
    connected=threading.Event();failed=[]
    client=mqtt.Client(mqtt.CallbackAPIVersion.VERSION2,client_id='iot-operator-'+os.urandom(4).hex())
    client.username_pw_set(os.environ['MQTT_USERNAME'],os.environ['MQTT_PASSWORD'])
    context=ssl.create_default_context(cafile=os.getenv('MQTT_CA_FILE') or None);client.tls_set_context(context)
    def on_connect(client,userdata,flags,reason,properties):
        if reason.is_failure:failed.append(str(reason))
        connected.set()
    client.on_connect=on_connect
    client.connect(host,port);client.loop_start()
    try:
        if not connected.wait(20):raise RuntimeError('connection timeout')
        if failed:raise RuntimeError('MQTT connection rejected: '+failed[0])
        info=client.publish(f'iot/{a.gateway}/commands',text,qos=1,retain=False)
        info.wait_for_publish(timeout=20)
        if not info.is_published():raise RuntimeError('publish acknowledgement timeout')
        print('Broker acknowledged command. Watch iot/'+a.gateway+'/ota/status for device result.')
    finally:client.disconnect();client.loop_stop()

if __name__=='__main__':main()
