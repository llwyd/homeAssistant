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
    reading = Readings(deviceID = name, datestamp = datestamp, timestamp = timestamp, temperature = temperature, humidity = humidity)
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
        data['uptime_ms'] = str(dt.timedelta(seconds=uptime_ms))
        new_key = key_str.removeprefix('flask_cache_')
        data_to_add[new_key] = data
        data_list.append(data_to_add)
    
    return data_list

def generate_temp_graph():
    fig = Figure()
    ax = fig.subplots()

    todays_date            = dt.datetime.now().strftime('%Y-%m-%d')
    yesterdays_date        = (dt.datetime.now() - dt.timedelta(days=1)).strftime('%Y-%m-%d') 
    yesterdays_time        = (dt.datetime.now() - dt.timedelta(days=1)) 
    
    todays_data = db.session.query( Readings ).filter( or_(Readings.datestamp == todays_date, Readings.datestamp == yesterdays_date) ).all()
#    for d in todays_data:
#        d_time = dt.datetime.strptime(d.datestamp + " " + d.timestamp,'%Y-%m-%d %H:%M')
#        if(d_time >= yesterdays_time):
#            if(d.deviceID=='utility_room'):
#                data['y'].append(d.temperature)
#                data['x'].append(d_time)
#            elif(d.deviceID=='outside'):
#                data['z'].append(d.temperature)
#                data['t'].append(d_time)
#            elif(d.deviceID=='lounge_area'):
#                data['p'].append(d.temperature)
#                data['s'].append(d_time)
#            elif(d.deviceID=='bedroom_area'):
#                data['c'].append(d.temperature)
#                data['d'].append(d_time)
    #ax.plot(x, label='Sine 1')
    #ax.plot(y, label='Sine 2')
    #ax.plot(z, label='Sine 3')
    
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
    graph_image = generate_test_graph()
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


#def mqtt_subscribe_all():
#    # Subscribe to defaults
#    # Note, shouldn't have to do it this way
#    mqtt.subscribe('home/utility_room/#')
#    mqtt.subscribe('home/lounge_area/#')
#    mqtt.subscribe('home/bedroom_area/#')
#
#
#basedir = os.path.abspath(os.path.dirname(__file__))
#
#external_stylesheets = ['https://codepen.io/chriddyp/pen/bWLwgP.css']
#
#server = Flask(__name__);
#app = dash.Dash(__name__,server=server, external_stylesheets=external_stylesheets)
#
#app.config.suppress_callback_exceptions = True
#
#app.title='Home Assistant'
#app.server.config["DEBUG"] = False
#
#cache_config = {
#    "DEBUG": True,         
#    "CACHE_TYPE": 'redis',
#    "CACHE_REDIS_HOST": 'localhost',
#    "CAHCE_REDIS_URL":'redis://localhost:6379',
#    "CACHE_DEFAULT_TIMEOUT": 300
#}
#
#app.server.config.from_mapping(cache_config)
#cache = Cache(app.server)
#
#app.server.config['MQTT_BROKER_URL'] = 'localhost' 
#app.server.config['MQTT_CLIENT_ID'] = 'pi-homepage'
#app.server.config['MQTT_BROKER_PORT'] = 1883 
#app.server.config['MQTT_USERNAME'] = ''
#app.server.config['MQTT_PASSWORD'] = '' 
#app.server.config['MQTT_KEEPALIVE'] = 60
#app.server.config['MQTT_TLS_ENABLED'] = False
#
## STart MQTT Connection
#mqtt = Mqtt(app=app.server,connect_async=True)
#
#SQLALCHEMY_DATABASE_URI = os.environ.get('DATABASE_URL') or 'sqlite:///' + os.path.join(basedir, 'app.db')
#app.server.config["SQLALCHEMY_DATABASE_URI"] = SQLALCHEMY_DATABASE_URI
#app.server.config["SQLALCHEMY_POOL_RECYCLE"] = 299
#app.server.config["SQLALCHEMY_TRACK_MODIFICATIONS"] = False
#
#db = SQLAlchemy(app.server)
#
## Database model
#class Readings(db.Model):
#    __tablename__ = "readings"
#    id              = db.Column(db.Integer, primary_key=True)       #id for each record in the database
#    deviceID        = db.Column(db.String(32))                      # id of the unit
#    datestamp       = db.Column(db.String(32))                     # datestamp
#    timestamp       = db.Column(db.String(32))                     # timestamp
#    temperature     = db.Column(db.String(16))                      # temperature
#
##   Entry point for the website
#app.layout = html.Div(children=[
#
#    dcc.Location(id='url', refresh=False),
#    html.Div(id='page_content')
#])
#
#
##   URL Handler
#@app.callback(Output('page_content', 'children'),[Input('url', 'pathname')])
#def display_page(pathname):
#    if pathname == '/live':
#        return live_page
#    else:
#        return index_page
#
##   Static Index Page
#index_page = html.Div([
#    html.H1(children='H O M E'),
#    html.Div(id = 'weather-description'),
#   dcc.Interval(   id='interval-component',
#                    interval = 1000 * 60 * 5,
#                    n_intervals = 0
#                ),
#    dcc.Graph(id='static-temp-graph',className='tempGraph')
#])
#
#stats_page = html.Div([
#    html.H1(children='H O M E'),
#   html.Div(id = 'last-update'), 
#    html.Div(id = 'server-uptime'),
#    html.Div(id = 'database-size'),
#   html.Div(id = 'cpu-temp'),
#   dcc.Interval(   id='interval-component',
#                    interval = 1000 * 60 * 5,
#                    n_intervals = 0
#                ),
#])
#
#live_page = html.Div([
#    html.H1(children='H O M E'),
#   html.Div(id = 'inside-temp'), 
#   html.Div(id = 'outside-temp'), 
#   html.Div(id = 'outside-desc'), 
#   html.Div(id = 'daemon-version'), 
#   dcc.Interval(   id='interval-component',
#                    interval = 1000 * 60 * 5,
#                    n_intervals = 0
#                ),
#])
#
## weather description
#@app.callback(Output('weather-description', 'children'),[Input('interval-component','n_intervals')])
#def update_uptime(n):
#    if( cache.get("weather_description") is not None):
#        weather_description = cache.get("weather_description")
#        return [ html.Span("Weather: " + str(weather_description))]    
#    else:
#        return [ html.Span("Weather: No idea :(")] 
#
##   Temperature graph
#@app.callback(Output('static-temp-graph', 'figure'),[Input('interval-component','n_intervals')])
#def static_temp_graph(value):
#    todays_date            = dt.datetime.now().strftime('%Y-%m-%d')
#    yesterdays_date        = (dt.datetime.now() - dt.timedelta(days=1)).strftime('%Y-%m-%d') 
#
#    yesterdays_time        = (dt.datetime.now() - dt.timedelta(days=1)) 
#    
#    todays_data = db.session.query( Readings ).filter( or_(Readings.datestamp == todays_date, Readings.datestamp == yesterdays_date) ).all()
#    data = {
#            'x':[],
#            'y':[],
#            'p':[],
#            's':[],
#            'c':[],
#            'd':[],
#            't':[],
#            'z':[],}
#
#    for d in todays_data:
#        d_time = dt.datetime.strptime(d.datestamp + " " + d.timestamp,'%Y-%m-%d %H:%M')
#        if(d_time >= yesterdays_time):
#            if(d.deviceID=='utility_room'):
#                data['y'].append(d.temperature)
#                data['x'].append(d_time)
#            elif(d.deviceID=='outside'):
#                data['z'].append(d.temperature)
#                data['t'].append(d_time)
#            elif(d.deviceID=='lounge_area'):
#                data['p'].append(d.temperature)
#                data['s'].append(d_time)
#            elif(d.deviceID=='bedroom_area'):
#                data['c'].append(d.temperature)
#                data['d'].append(d_time)
#    figure={
#        'data': [
#            {'x': data['x'], 'y': data['y'], 'type': 'line', 'name': 'Utility Room'},
#            {'x': data['t'], 'y': data['z'], 'type': 'line', 'name': 'Outside'},
#            {'x': data['s'], 'y': data['p'], 'type': 'line', 'name': 'Lounge'},
#            {'x': data['d'], 'y': data['c'], 'type': 'line', 'name': 'Bedroom'}
#        ],
#        'layout': {
#           'height': 700,
#            'plot_bgcolor': 'rgba(0,0,0,0)',
#            'paper_bgcolor': 'rgba(0,0,0,0)',
#            'legend':dict(font=dict(color='#386ddb')),
#            'xaxis': {
#                'title': 'Time',
#                'showgrid': False,
#                'color':'#386ddb'
#            },
#            'yaxis': {
#                'title': 'Temperature (Celsius)',
#                'showgrid': False,
#                'color':'#386ddb'
#            }
#        },
#
#    }
#    return figure;
#
## debug dump all of the data
#@app.server.route('/dump', methods = ['GET', 'POST'])
#def dump_data():
#    return render_template("raw_data.html", readings = Readings.query.all())
#
## inside-temp
#@app.callback(Output('inside-temp', 'children'),[Input('interval-component','n_intervals')])
#def update_inside_temp(n):
#    if( cache.get("inside_temp") is not None):
#        inside_temp = cache.get("inside_temp")
#        return [ html.Span("inside_temp: " + str(inside_temp) + " C")] 
#    else:
#        return [ html.Span("inside_temp: ????")]   
#
## daemon-version
#@app.callback(Output('daemon-version', 'children'),[Input('interval-component','n_intervals')])
#def update_daemon_version(n):
#    if( cache.get("daemon_version") is not None):
#        daemon_version = cache.get("daemon_version")
#        return [ html.Span("daemon_version: " + str(daemon_version))]  
#    else:
#        return [ html.Span("daemon_version: ????")]    
#
#
## daemon-location
#@app.callback(Output('daemon-location', 'children'),[Input('interval-component','n_intervals')])
#def update_daemon_location(n):
#    if( cache.get("daemon_location") is not None):
#        daemon_location = cache.get("daemon_location")
#        return [ html.Span("Location: " + str(daemon_location))]   
#    else:
#        return [ html.Span("Location: ????")]  
#
## A bug, this callback DOES NOT call, even with connect_async=True
#@mqtt.on_connect()
#def handle_connect(client,userdata,flags,rc):
#    print("Hello")
#    mqtt.subscribe('home/lounge_area/node_temp')
#    mqtt.subscribe('home/lounge_area/out_temp')
#    mqtt.subscribe('home/lounge_area/out_desc')
#    mqtt.subscribe('home/bedroom_area/node_temp')
#    mqtt.subscribe('home/utility_room/node_temp')
#
#def database_update(name, temperature):
#    datestamp = dt.datetime.now().strftime('%Y-%m-%d')
#    timestamp = dt.datetime.now().strftime('%H:%M')
#    reading = Readings(    deviceID = name, datestamp = datestamp, timestamp = timestamp, temperature  = temperature)
#    db.session.add(reading)
#    db.session.commit()
#
#@mqtt.on_topic('home/utility_room/#')
#def handle_utility_room_data(client,userdata,message):
#    full_topic = message.topic
#    topic = full_topic.replace('/',' ').split()[-1]
#    data = message.payload.decode()
#    if topic == 'node_temp':
#        database_update("utility_room", data)
#
#@mqtt.on_topic('home/lounge_area/#')
#def handle_lounge_area_data(client,userdata,message):
#    full_topic = message.topic
#    topic = full_topic.replace('/',' ').split()[-1]
#    data = message.payload.decode()
#    if topic == 'node_temp':
#        database_update("lounge_area", data)
#    elif topic == 'out_temp':
#        database_update("outside", data)
#    elif topic == 'out_desc':    
#        cache.set("weather_description",str(data).capitalize()) 
#
#@mqtt.on_topic('home/bedroom_area/#')
#def handle_bedroom_area_data(client,userdata,message):
#    full_topic = message.topic
#    topic = full_topic.replace('/',' ').split()[-1]
#    data = message.payload.decode()
#    if topic == 'node_temp':
#        database_update("bedroom_area", data)
#
#@mqtt.on_message()
#def handle_mqtt_message(client,userdata,message):
#    mqtt.publish('home/debug','Received!')
#
#@mqtt.on_log()
#def handle_logging(client,userdata,level,buf):
#    print(client,userdata,level,buf)
#
#if __name__ == '__main__':
#    app.run_server(host='0.0.0.0', use_reloader=False)
