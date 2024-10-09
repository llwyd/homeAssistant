from flask import render_template
from app import app

@app.route("/")
def index():
    return "Hello World"

@app.route('/dump')
def dump_data():
    return "Dumped Data"
