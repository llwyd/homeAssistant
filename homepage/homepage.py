from flask import Flask, redirect, render_template, request, url_for
from flask_sqlalchemy import SQLAlchemy
#from flask_basicauth import BasicAuth
from flask_caching import Cache
from flask_mqtt import Mqtt
from sqlalchemy import or_
from matplotlib.figure import Figure
import base64
from io import BytesIO
import datetime as dt
import numpy as np
import os
import subprocess
import json
import redis
import pandas as pd

app = Flask(__name__)

basedir = os.path.abspath(os.path.dirname(__file__))
SQLALCHEMY_DATABASE_URI = os.environ.get('DATABASE_URL') or 'sqlite:///' + os.path.join(basedir, 'app.db')
app.config["SQLALCHEMY_DATABASE_URI"] = SQLALCHEMY_DATABASE_URI
app.config["SQLALCHEMY_POOL_RECYCLE"] = 299
app.config["SQLALCHEMY_TRACK_MODIFICATIONS"] = False
db = SQLAlchemy(app)

class EnvironmentData(db.Model):
    __tablename__ = "environment_data"
    id              = db.Column(db.Integer, primary_key=True)
    device_id       = db.Column(db.String(32))
    datestamp       = db.Column(db.String(32))
    timestamp       = db.Column(db.String(32))
    temperature     = db.Column(db.String(16))
    humidity        = db.Column(db.String(16))

#class HumidityData(db.Model):
#    __tablename__ = "humidity_data"
#    id              = db.Column(db.Integer, primary_key=True)
#    device_id       = db.Column(db.String(32))                      # id of the unit
#    datestamp       = db.Column(db.String(32))                     # datestamp
#    timestamp       = db.Column(db.String(32))                     # timestamp
#    humidity        = db.Column(db.String(16))                      # humidity

cache_config = {
    "DEBUG": True,         
    "CACHE_TYPE": 'redis',
    "CACHE_REDIS_HOST": 'localhost',
    "CAHCE_REDIS_URL":'redis://localhost:6379',
    "CACHE_DEFAULT_TIMEOUT": 300
}

app.config.from_mapping(cache_config)
cache = Cache(app)

cache.delete('temperature')

app.config['MQTT_BROKER_URL'] = 'localhost' 
app.config['MQTT_CLIENT_ID'] = 'pi-homepage'
app.config['MQTT_BROKER_PORT'] = 1883 
app.config['MQTT_USERNAME'] = ''
app.config['MQTT_PASSWORD'] = '' 
app.config['MQTT_KEEPALIVE'] = 60
app.config['MQTT_TLS_ENABLED'] = False

# Start MQTT Connection
mqtt = Mqtt(app=app,connect_async=True)

last_update = "N/A"
site_version = 3.0

def database_update(name, temperature, humidity):
    datestamp = dt.datetime.now().strftime('%Y-%m-%d')
    timestamp = dt.datetime.now().strftime('%H:%M')
    reading = EnvironmentData(device_id = name, datestamp = datestamp, timestamp = timestamp, temperature = temperature, humidity = humidity)
    db.session.add(reading)
    db.session.commit()

def generate_test_graph():
    fig = Figure()
    ax = fig.subplots()
    
    f = 10 #Hz
    fs = 11250 #Hz
    t = np.linspace(0,fs,fs)
    x = np.sin( 2 * np.pi * f * (1/fs)* t )
    y = np.sin( (2 * np.pi * f * (1/fs)* t) + (np.pi/3) )
    z = np.sin( (2 * np.pi * f * (1/fs)* t) + ((2*np.pi)/3) )

    ax.plot(x, label='Sine 1')
    ax.plot(y, label='Sine 2')
    ax.plot(z, label='Sine 3')

    ax.set_title('Sine waves')
    ax.set_xlabel('Samples')
    ax.set_ylabel('Amplitude')
    ax.legend()

    buf = BytesIO()
    fig.savefig(buf, format='png')

    return base64.b64encode(buf.getbuffer()).decode("ascii")

def get_nodes():
    r = redis.StrictRedis(host='localhost', port=6379, db=0)
    key_list = []
    for key in r.scan_iter("flask_cache_*"):
        key_str = key.decode()
        new_key = key_str.removeprefix('flask_cache_')
        key_list.append(new_key)
    return key_list

def generate_temp_graph():
    fig = Figure()
    ax = fig.subplots()

    todays_date            = dt.datetime.now().strftime('%Y-%m-%d')
    yesterdays_date        = (dt.datetime.now() - dt.timedelta(days=1)).strftime('%Y-%m-%d') 
    yesterdays_time        = (dt.datetime.now() - dt.timedelta(days=1)) 
   
    nodes = get_nodes()

    for node in nodes:
        todays_data = db.session.query(EnvironmentData).filter( or_(EnvironmentData.datestamp == todays_date, EnvironmentData.datestamp == yesterdays_date), EnvironmentData.device_id == node ).all()
        x=[]
        t=[]
        for d in todays_data:
            d_time = dt.datetime.strptime(d.datestamp + " " + d.timestamp,'%Y-%m-%d %H:%M')
            if(d_time >= yesterdays_time):
                x.append(d.temperature)
                t.append(d_time)


        ax.plot(t,x,label=node)
    
    ax.set_title('Temperature')
    ax.set_xlabel('Time')
    ax.set_ylabel('Degrees (C)')
    ax.legend()

    buf = BytesIO()
    fig.savefig(buf, format='png')
    return base64.b64encode(buf.getbuffer()).decode("ascii")


def generate_data_graphs():
    fig = Figure()
    ax = fig.subplots()

    todays_date            = dt.datetime.now().strftime('%Y-%m-%d')
    yesterdays_date        = (dt.datetime.now() - dt.timedelta(days=1)).strftime('%Y-%m-%d') 
    yesterdays_time        = (dt.datetime.now() - dt.timedelta(days=1)) 
    
    todays_data = db.session.query(EnvironmentData).filter( or_(EnvironmentData.datestamp == todays_date, EnvironmentData.datestamp == yesterdays_date)).all()

    df = pd.DataFrame(todays_data)
    df['temperature'] = df['temperature'].astype(float)
    df['humidity'] = df['humidity'].astype(float)
    nodes = df.device_id.unique()

    for node in nodes:
        data = df[df['device_id'] == node]
        data = data[data['datestamp'] == test_date]
        data['datetime'] = data['datestamp'] + " " + data['timestamp'] 
        d_time = [dt.datetime.strptime(date,'%Y-%m-%d %H:%M') for date in data['datetime']]
        ax.plot(d_time, np.float32(data['temperature']),label=node)
    
    ax.set_title('Temperature')
    ax.set_xlabel('Time')
    ax.set_ylabel('Degrees (C)')
    ax.legend()

    buf = BytesIO()
    fig.savefig(buf, format='png')
    return base64.b64encode(buf.getbuffer()).decode("ascii")

def get_environment_data():
    r = redis.StrictRedis(host='localhost', port=6379, db=0)
    data_list = []
    for key in r.scan_iter("flask_cache_*"):
        data_to_add = {}
        key_raw = key[0]
        key_str = key.decode()
        data = cache.get(key_str.removeprefix('flask_cache_'))
        uptime_ms = int(data['uptime_ms'] / 1000)
        data['uptime_ms'] = str(dt.timedelta(seconds=uptime_ms))
        new_key = key_str.removeprefix('flask_cache_')
        data_to_add[new_key] = data
        data_list.append(data_to_add)
    
    return data_list

def get_uptime():
    uptime_result = subprocess.run(['uptime','--pretty'], stdout=subprocess.PIPE)
    return uptime_result.stdout.decode('utf-8')[3:-1];

@app.route("/")
def index():
    graph_image = generate_data_graphs()
    env_data = get_environment_data()
    last_update = dt.datetime.now().strftime('%Y-%m-%d %H-%M-%S')

    uptime = get_uptime()

    return render_template('index.html',
                           graph=graph_image,
                           last_update=last_update,
                           site_version=site_version,
                           uptime=uptime,
                           environment=env_data)

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
    if( cache.get(node) is not None):
        temp = cache.get(node)
        temp = data
        cache.set(node,temp)
    else:
        temp = data
        cache.set(node,temp)

@mqtt.on_topic('home/summary/#')
def handle_summary_data(client,userdata,message):
    full_topic = message.topic
    node = full_topic.replace('/',' ').split()[-1]
    data_str = message.payload.decode()
    data = json.loads(data_str)
    database_update(node, data['temperature'], data['humidity'])

@mqtt.on_message()
def handle_mqtt_message(client,userdata,message):
    mqtt.publish('home/debug','Received!')

if __name__ == '__main__':
    app.run_server(host='0.0.0.0', use_reloader=False)

