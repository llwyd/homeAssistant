from flask import render_template
from sqlalchemy import or_
from app import app, db
from app.models import EnvironmentData
import datetime as dt

@app.route("/")
def index():
    return "Hello World"

@app.route('/dump')
def dump_data():
    todays_date            = dt.datetime.now().strftime('%Y-%m-%d')
    yesterdays_date        = (dt.datetime.now() - dt.timedelta(days=1)).strftime('%Y-%m-%d') 
    yesterdays_time        = (dt.datetime.now() - dt.timedelta(days=1)) 
   
    with app.app_context():
        todays_data = db.session.query(EnvironmentData).filter( or_(EnvironmentData.datestamp == todays_date, EnvironmentData.datestamp == yesterdays_date)).all() 
    return render_template("raw_data.html", readings=todays_data)
