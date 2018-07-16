from flask import Flask, render_template, request
from pprint import pprint

app = Flask(__name__)

@app.route("/")
def index():
    return render_template("index.html")


@app.route("/update")
def update():
    print(pprint(request.args))
    
    return "No update available", 304

