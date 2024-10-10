import sqlalchemy as sa
import sqlalchemy.orm as so
from app import db

class EnvironmentData(db.Model):
    __tablename__ = "environment_data"
    id              = db.Column(db.Integer, primary_key=True)
    device_id       = db.Column(db.String(32))
    datestamp       = db.Column(db.String(32))
    timestamp       = db.Column(db.String(32))
    temperature     = db.Column(db.String(16))
    humidity        = db.Column(db.String(16))

    def __init__(self, device_id=None, datestamp=None, timestamp=None, temperature=None, humidity=None):
        self.data = (device_id, datestamp, timestamp, temperature, humidity)
        self.device_id = device_id
        self.datestamp = datestamp
        self.timestamp = timestamp
        self.temperature = temperature
        self.humidity = humidity

    def __repr__(self):
        return (self.device_id, self.datestamp, self.timestamp, self.temperature, self.humidity)
