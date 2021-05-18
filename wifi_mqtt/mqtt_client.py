import paho.mqtt.client as paho
import time

import serial
serdev = '/dev/ttyACM1'
s = serial.Serial(serdev, 9600)

# https://os.mbed.com/teams/mqtt/wiki/Using-MQTT#python-client

# MQTT broker hosted on local machine
mqttc = paho.Client()

tilt_over_number=0
confirm_degree=""
exceed_x_angle=['','','','','','','','','','','']
exceed_y_angle=['','','','','','','','','','','']
exceed_z_angle=['','','','','','','','','','','']
exceed_angle=['','','','','','','','','','','']

# Settings for connection
# TODO: revise host to your IP
host = "172.24.2.86"
topic = "Mbed"

# Callbacks
def on_connect(self, mosq, obj, rc):
    print("Connected rc: " + str(rc))

def on_message(mosq, obj, msg):
    print("[Received] Topic: " + msg.topic + ", Message: " + str(msg.payload) + "\n")
    #mqttc.publish(topic, "Message from Python!\n", qos=0)
    #if msg.topic== topic:
    #mqttc.publish("ACK", "Ack from Python!\n", qos=1)
    #string=msg.payload
    #x= ''.join([x for x in string if x.isdigit()])
    #x=x>>2
    mode=str(msg.payload)[8:9]
    print("mode: "+mode+"\n")
    if (mode=='1'):
        global confirm_degree
        confirm_degree=str(msg.payload)[19:21]
        print("---------------Confirmed Degree information--------------\n")
        print("confirm_degree="+confirm_degree)
        print("---------------------------------------------------------\n")
        time.sleep(1)
        s.write(bytes("/STOPGESTURE/run\n", 'UTF-8'))
        print("let's goog\n")
    #line=s.readline()
    #print(line)
    #line=s.readline()
    #print(line)
        time.sleep(10)
        s.write(bytes("/RECEIVECONFIRMEDDEGREE/run "+confirm_degree+"\n", 'UTF-8'))
        print("/RECEIVECONFIRMEDDEGREE/run "+confirm_degree+"\n")
        print("sendback confirm_degree!!\n")
    if (mode=='2'):
        print("tilt_+1")
        global tilt_over_number
        #exceed_x_angle[tilt_over_number]=str(msg.payload)[11:15]
        #exceed_y_angle[tilt_over_number]=str(msg.payload)[16:20]
        #exceed_z_angle[tilt_over_number]=str(msg.payload)[21:25]
        exceed_x_angle[tilt_over_number]=str(msg.payload)[11:16]
        exceed_y_angle[tilt_over_number]=str(msg.payload)[17:22]
        exceed_z_angle[tilt_over_number]=str(msg.payload)[23:28]
        exceed_angle[tilt_over_number]=str(msg.payload)[30:35]
        #print(exceed_x_angle+"\n")
        #print(exceed_y_angle+"\n")
        #print(exceed_z_angle+"\n")
        #print("Degree exceeded!! ("+exceed_x_angle+", "+exceed_y_angle+", "+exceed_z_angle+")\n")
        #print("Degree exceeded!! ("+exceed_x_angle[tilt_over_number]+", "+exceed_y_angle[tilt_over_number]+", "+exceed_z_angle[tilt_over_number]+")\n")
        print("Degree exceeded!! ("+exceed_x_angle[tilt_over_number]+", "+exceed_y_angle[tilt_over_number]+", "+exceed_z_angle[tilt_over_number]+")")
        print("Angle: %s\n" % exceed_angle[tilt_over_number])
        #tilt_over_number=tilt_over_number+1
        #global tilt_over_number #declare 'tilt_over_number' used in this function is the global one 
        tilt_over_number=tilt_over_number+1
        #tilt_over_number += 1
        #print("Tilt over "+tilt_over_number+" time(s)!!\n")
        print("Tilt over %d time(s)!!\n" % tilt_over_number)
        if (tilt_over_number>9):
            #time.sleep(1)
            s.write(bytes("/STOPTILT/run\n", 'UTF-8'))
            time.sleep(10)
            #global confirm_degree
            s.write(bytes("/RECEIVECONFIRMEDDEGREE/run "+confirm_degree+"\n", 'UTF-8'))
            print("/RECEIVECONFIRMEDDEGREE/run "+confirm_degree+"\n")
            print("sendback confirm_degree!!\n")
            print("------------------Tilt over information------------------\n")
            for i in range(10):
                #print("Number: %d, ( "+exceed_x_angle[i]+", "+exceed_y_angle[i]+", "+exceed_z_angle[i]+")\n" % i)
                print("Number: %d, ( %s, %s, %s) ; Angle: %s\n" % (i+1, exceed_x_angle[i], exceed_y_angle[i],  exceed_z_angle[i], exceed_angle[i]))
            print("---------------------------------------------------------\n")
    if (mode=='3'):
        tilt_over_number=0



def on_subscribe(mosq, obj, mid, granted_qos):
    print("Subscribed OK")

def on_unsubscribe(mosq, obj, mid, granted_qos):
    print("Unsubscribed OK")

# Set callbacks
mqttc.on_message = on_message
mqttc.on_connect = on_connect
mqttc.on_subscribe = on_subscribe
mqttc.on_unsubscribe = on_unsubscribe

# Connect and subscribe
print("Connecting to " + host + "/" + topic)
mqttc.connect(host, port=1883, keepalive=60)
mqttc.subscribe(topic, 0)

# Publish messages from Python
#num = 0
#while num != 5:
#    ret = mqttc.publish(topic, "Message from Python!\n", qos=0)
#    if (ret[0] != 0):
#            print("Publish failed")
#    mqttc.loop()
#    time.sleep(1.5)
#    num += 1

# Loop forever, receiving messages
mqttc.loop_forever()