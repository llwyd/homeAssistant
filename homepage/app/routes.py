from flask import render_template
from sqlalchemy import or_
from app import app, db
from app.models import EnvironmentData
import datetime as dt
import pandas as pd
from matplotlib.figure import Figure
import base64
from io import BytesIO
import numpy as np
import subprocess

def get_todays_data():
    todays_date            = dt.datetime.now().strftime('%Y-%m-%d')
    yesterdays_date        = (dt.datetime.now() - dt.timedelta(days=1)).strftime('%Y-%m-%d') 
    yesterdays_time        = (dt.datetime.now() - dt.timedelta(days=1)) 
   
    with app.app_context():
        todays_data = db.session.query(EnvironmentData).filter( or_(EnvironmentData.datestamp == todays_date, EnvironmentData.datestamp == yesterdays_date)).all() 
    return todays_data

def generate_data_graph(todays_data):
    fig = Figure()
    ax = fig.subplots()
    df = pd.DataFrame([(data.device_id, data.datestamp, data.timestamp, data.temperature, data.humidity) for data in todays_data], columns=['device_id','datestamp','timestamp','temperature','humidity'])

    df['temperature'] = df['temperature'].astype(float)
    df['humidity'] = df['humidity'].astype(float)
    nodes = df.device_id.unique()

    for node in nodes:
        data = df.copy()
        data = data[data['device_id'] == node]
        data['datetime'] = data['datestamp'] + " " + data['timestamp'] 
        d_time = [dt.datetime.strptime(date,'%Y-%m-%d %H:%M') for date in data['datetime']]
        ax.plot(d_time, np.float32(data['temperature']),label=node)
    
    ax.set_title('Temperature')
    ax.set_xlabel('Time')
    ax.set_ylabel('Degrees (C)')
    ax.tick_params('x', labelrotation=45)
    ax.legend()

    buf = BytesIO()
    fig.savefig(buf, format='png')
    return base64.b64encode(buf.getbuffer()).decode("ascii")

def generate_humidity_graph(todays_data):
    fig = Figure()
    ax = fig.subplots()
    df = pd.DataFrame([(data.device_id, data.datestamp, data.timestamp, data.temperature, data.humidity) for data in todays_data], columns=['device_id','datestamp','timestamp','temperature','humidity'])

    df['temperature'] = df['temperature'].astype(float)
    df['humidity'] = df['humidity'].astype(float)
    nodes = df.device_id.unique()

    for node in nodes:
        data = df.copy()
        data = data[data['device_id'] == node]
        data['datetime'] = data['datestamp'] + " " + data['timestamp'] 
        d_time = [dt.datetime.strptime(date,'%Y-%m-%d %H:%M') for date in data['datetime']]
        ax.plot(d_time, np.float32(data['humidity']),label=node)
    
    ax.set_title('Humidity')
    ax.set_xlabel('Time')
    ax.set_ylabel('Relative Humidity')
    ax.tick_params('x', labelrotation=45)
    ax.legend()

    buf = BytesIO()
    fig.savefig(buf, format='png')
    return base64.b64encode(buf.getbuffer()).decode("ascii")

def get_uptime():
    uptime_result = subprocess.run(['uptime','--pretty'], stdout=subprocess.PIPE)
    return uptime_result.stdout.decode('utf-8')[3:-1];

@app.route("/")
def index():
    
    todays_data = get_todays_data()
    temperature_graph = generate_data_graph(todays_data)    
    humidity_graph = generate_humidity_graph(todays_data)    
    last_update = dt.datetime.now().strftime('%Y-%m-%d %H-%M-%S')
    uptime = get_uptime()
    return render_template('index.html',
                           graph=temperature_graph,
                           hum_graph=humidity_graph,
                           last_update=last_update,
                           uptime=uptime)

@app.route('/dump')
def dump_data():
    todays_data = get_todays_data()
    return render_template("raw_data.html", readings=todays_data)
