import os

basedir = os.path.abspath(os.path.dirname(__file__))
class Config:
    SQLALCHEMY_DATABASE_URI = os.environ.get('DATABASE_URL') or 'sqlite:///' + os.path.join(basedir, 'app.db')
    SQLALCHEMY_POOL_RECYCLE = 299
    SQLALCHEMY_TRACK_MODIFICATIONS = False
    MQTT_BROKER_URL = 'localhost' 
    MQTT_CLIENT_ID = 'pi-homepage'
    MQTT_BROKER_PORT = 1883 
    MQTT_USERNAME = ''
    MQTT_PASSWORD = '' 
    MQTT_KEEPALIVE = 60
    MQTT_TLS_ENABLED = False

