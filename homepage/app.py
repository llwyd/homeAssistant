from flask import Flask, redirect, render_template, request, url_for
from flask_sqlalchemy import SQLAlchemy
from flask_basicauth import BasicAuth
from flask_caching import Cache
from flask_mqtt import Mqtt
from sqlalchemy import or_
import datetime as dt
import dash
import dash_core_components as dcc
import dash_html_components as html
import plotly.graph_objs as go
import plotly
import devices as dv
from dash.dependencies import Input, Output
import numpy as np
import os


def mqtt_subscribe_all():
    # Subscribe to defaults
    # Note, shouldn't have to do it this way
    mqtt.subscribe('home/livingroom/#')
    mqtt.subscribe('home/inside_node/#')


basedir = os.path.abspath(os.path.dirname(__file__))

external_stylesheets = ['https://codepen.io/chriddyp/pen/bWLwgP.css']

server = Flask(__name__);
app = dash.Dash(__name__,server=server, external_stylesheets=external_stylesheets)

app.config.suppress_callback_exceptions = True

app.title='Home Assistant'
app.server.config["DEBUG"] = True

cache_config = {
    "DEBUG": True,         
    "CACHE_TYPE": 'redis',
    "CACHE_REDIS_HOST": 'localhost',
    "CAHCE_REDIS_URL":'redis://localhost:6379',
    "CACHE_DEFAULT_TIMEOUT": 300
}

app.server.config.from_mapping(cache_config)
cache = Cache(app.server)

app.server.config['MQTT_BROKER_URL'] = 'localhost' 
app.server.config['MQTT_CLIENT_ID'] = 'pi-homepage'
app.server.config['MQTT_BROKER_PORT'] = 1883 
app.server.config['MQTT_USERNAME'] = ''
app.server.config['MQTT_PASSWORD'] = '' 
app.server.config['MQTT_KEEPALIVE'] = 60
app.server.config['MQTT_TLS_ENABLED'] = False

# STart MQTT Connection
mqtt = Mqtt(app=app.server,connect_async=False)

mqtt_subscribe_all()

SQLALCHEMY_DATABASE_URI = os.environ.get('DATABASE_URL') or 'sqlite:///' + os.path.join(basedir, 'app.db')
app.server.config["SQLALCHEMY_DATABASE_URI"] = SQLALCHEMY_DATABASE_URI
app.server.config["SQLALCHEMY_POOL_RECYCLE"] = 299
app.server.config["SQLALCHEMY_TRACK_MODIFICATIONS"] = False

db = SQLAlchemy(app.server)

# Database model
class Readings(db.Model):
    __tablename__ = "readings"
    id              = db.Column(db.Integer, primary_key=True)       #id for each record in the database
    deviceID        = db.Column(db.String(32))                      # id of the unit
    datestamp       = db.Column(db.String(32))						# datestamp
    timestamp       = db.Column(db.String(32))						# timestamp
    temperature     = db.Column(db.String(16))                      # temperature

#   Entry point for the website
app.layout = html.Div(children=[

    dcc.Location(id='url', refresh=False),
    html.Div(id='page_content')
])


#   URL Handler
@app.callback(Output('page_content', 'children'),[Input('url', 'pathname')])
def display_page(pathname):
    if pathname == '/live':
        return live_page
    else:
        return index_page

#   Static Index Page
index_page = html.Div([
    html.H1(children='H O M E'),
    html.Div(id = 'weather-description'),
	html.Div(id = 'daemon-version'), 
	html.Div(id = 'daemon-location'), 
	dcc.Interval(   id='interval-component',
                    interval = 1000 * 60 * 5,
                    n_intervals = 0
                ),
    dcc.Graph(id='static-temp-graph',className='tempGraph')
])

stats_page = html.Div([
    html.H1(children='H O M E'),
	html.Div(id = 'last-update'), 
    html.Div(id = 'server-uptime'),
    html.Div(id = 'database-size'),
	html.Div(id = 'cpu-temp'),
	dcc.Interval(   id='interval-component',
                    interval = 1000 * 60 * 5,
                    n_intervals = 0
                ),
])

live_page = html.Div([
    html.H1(children='H O M E'),
	html.Div(id = 'inside-temp'), 
	html.Div(id = 'outside-temp'), 
	html.Div(id = 'outside-desc'), 
	html.Div(id = 'daemon-version'), 
	dcc.Interval(   id='interval-component',
                    interval = 1000 * 60 * 5,
                    n_intervals = 0
                ),
])

# weather description
@app.callback(Output('weather-description', 'children'),[Input('interval-component','n_intervals')])
def update_uptime(n):
    if( cache.get("weather_description") is not None):
        weather_description = cache.get("weather_description")
        return [ html.Span("Weather: " + str(weather_description))]	
    else:
        return [ html.Span("Weather: No idea :(")]	

#   Temperature graph
@app.callback(Output('static-temp-graph', 'figure'),[Input('interval-component','n_intervals')])
def static_temp_graph(value):
    todays_date			= dt.datetime.now().strftime('%Y-%m-%d')
    yesterdays_date		= (dt.datetime.now() - dt.timedelta(days=1)).strftime('%Y-%m-%d') 

    yesterdays_time		= (dt.datetime.now() - dt.timedelta(days=1)) 
    
    todays_data = db.session.query( Readings ).filter( or_(Readings.datestamp == todays_date, Readings.datestamp == yesterdays_date) ).all()
    data = {
            'x':[],
            'y':[],
            'p':[],
            's':[],
            't':[],
            'z':[],}

    for d in todays_data:
        d_time = dt.datetime.strptime(d.datestamp + " " + d.timestamp,'%Y-%m-%d %H:%M')
        if(d_time >= yesterdays_time):
            if(d.deviceID=='livingroom'):
                data['y'].append(d.temperature)
                data['x'].append(d_time)
            elif(d.deviceID=='outside'):
                data['z'].append(d.temperature)
                data['t'].append(d_time)
            elif(d.deviceID=='inside_node'):
                data['p'].append(d.temperature)
                data['s'].append(d_time)
    figure={
        'data': [
            {'x': data['x'], 'y': data['y'], 'type': 'line', 'name': 'Inside'},
            {'x': data['t'], 'y': data['z'], 'type': 'line', 'name': 'Outside'},
            {'x': data['s'], 'y': data['p'], 'type': 'line', 'name': 'Node'}
        ],
        'layout': {
			'height': 700,
            'plot_bgcolor': 'rgba(0,0,0,0)',
            'paper_bgcolor': 'rgba(0,0,0,0)',
            'legend':dict(font=dict(color='#386ddb')),
            'xaxis': {
                'title': 'Time',
                'showgrid': False,
                'color':'#386ddb'
            },
            'yaxis': {
                'title': 'Temperature (Celsius)',
                'showgrid': False,
                'color':'#386ddb'
            }
        },

    }
    return figure;

# debug dump all of the data
@app.server.route('/dump', methods = ['GET', 'POST'])
def dump_data():
    return render_template("raw_data.html", readings = Readings.query.all())

# inside-temp
@app.callback(Output('inside-temp', 'children'),[Input('interval-component','n_intervals')])
def update_inside_temp(n):
    if( cache.get("inside_temp") is not None):
        inside_temp = cache.get("inside_temp")
        return [ html.Span("inside_temp: " + str(inside_temp) + " C")]	
    else:
        return [ html.Span("inside_temp: ????")]	

# daemon-version
@app.callback(Output('daemon-version', 'children'),[Input('interval-component','n_intervals')])
def update_daemon_version(n):
    if( cache.get("daemon_version") is not None):
        daemon_version = cache.get("daemon_version")
        return [ html.Span("daemon_version: " + str(daemon_version))]	
    else:
        return [ html.Span("daemon_version: ????")]	


# daemon-location
@app.callback(Output('daemon-location', 'children'),[Input('interval-component','n_intervals')])
def update_daemon_location(n):
    if( cache.get("daemon_location") is not None):
        daemon_location = cache.get("daemon_location")
        return [ html.Span("Location: " + str(daemon_location))]	
    else:
        return [ html.Span("Location: ????")]	

# A bug, this callback DOES NOT call, even with connect_async=True
@mqtt.on_connect()
def handle_connect(client,userdata,flags,rc):
    mqtt.publish('livingroom/homepage_status','Connected!')
    mqtt_subscribe_all()

def database_update(name, temperature):
    datestamp = dt.datetime.now().strftime('%Y-%m-%d')
    timestamp = dt.datetime.now().strftime('%H:%M')
    reading = Readings(	deviceID = name, datestamp = datestamp, timestamp = timestamp, temperature	= temperature)
    db.session.add(reading)
    db.session.commit()

@mqtt.on_topic('home/livingroom/#')
def handle_livingroom_data(client,userdata,message):
    full_topic = message.topic
    topic = full_topic.replace('/',' ').split()[-1]
    data = message.payload.decode()
    if topic == 'inside_temp_live':
        cache.set("inside_temp",str(data))
    elif topic == 'inside_temp':
        database_update("livingroom", data)
    elif topic == 'outside_temp_live':    
        cache.set("outside_temp",str(data)) 
    elif topic == 'outside_temp':    
        database_update("outside", data)
    elif topic == 'outside_desc':    
        cache.set("outside_desc",str(data)) 
        cache.set("weather_description",str(data)) 
    elif topic == 'daemon_version':    
        cache.set("daemon_version",str(data),timeout=7200) 
    elif topic == 'location':    
        cache.set("daemon_location",str(data),timeout=7200) 

@mqtt.on_topic('home/inside_node/#')
def handle_inside_node_data(client,userdata,message):
    full_topic = message.topic
    topic = full_topic.replace('/',' ').split()[-1]
    data = message.payload.decode()
    if topic == 'node_temp':
        database_update("inside_node", data)

@mqtt.on_message()
def handle_mqtt_message(client,userdata,message):
    mqtt.publish('home/debug','Received!')

#@mqtt.on_log()
#def handle_logging(client,userdata,level,buf):
#    print(client,userdata,level,buf)

if __name__ == '__main__':
    app.run_server(host='0.0.0.0')
