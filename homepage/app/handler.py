from app import app, db, mqtt
import json

@mqtt.on_connect()
def handle_connect(client,userdata,flags,rc):
    print("MQTT Connected")
    mqtt.subscribe('home/environment/#')
    mqtt.subscribe('home/summary/#')

@mqtt.on_topic('home/environment/#')
def handle_environment_data(client,userdata,message):
    full_topic = message.topic
    node = full_topic.replace('/',' ').split()[-1]
    data_str = message.payload.decode()
    data = json.loads(data_str)
    print(data)

