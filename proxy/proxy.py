#!/usr/bin/env python
# proxy.py
import argparse
import socket
from flask import Flask
from flask import render_template
from flask import request
from flask import redirect
from flask import url_for
from flask import send_from_directory
from flask import session as flask_session
from flask import make_response

dirAddr = None
cacheAddr = None
FILEMAX=1<<20

# What front end needs to do
# upload request to directory
# delete request to directory
# list from directory
# get content from cache

# Create the Flask application
app = Flask(__name__, static_url_path='')
app.secret_key = 'key'

@app.route('/')
def index():
    '''Display list of needles.'''
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(dirAddr)
    s.send(b'list\n')
    buff = s.recv(FILEMAX).splitlines()
    if buff and 'ok' in buff[0]:
        return render_template('index.html', needles=buff[1:])
    elif buff:
        return buff[0]
    else:
        return 'Unknown Error'

@app.route('/show/<int:needleId>')
def show(needleId):
    '''Displays the contents of a needle.'''
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(cacheAddr)
    s.send(b'get {}\n'.format(needleId))
    buff = s.recv(FILEMAX).splitlines()
    if buff and 'ok' in buff[0]:
        return render_template('data.html',
            data=''.join(buff[1:]), needleId=needleId)
    elif buff:
        return buff[0]
    else:
        return 'Unknown Error'

@app.route('/delete/<int:needleId>')
def delete(needleId):
    '''Removes a needle.'''
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(dirAddr)
    s.send(b'delete {}\n'.format(needleId))
    buff = s.recv(FILEMAX)
    if 'ok' in buff:
        return 'Needle {} has been deleted'.format(needleId)
    elif buff:
        return buff
    else:
        return 'Unknown Error'

@app.route('/upload', methods=['POST'])
def upload():
    '''Uploads a new document.'''
    f = request.files['file']
    blob = f.read()
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(dirAddr)
    s.send(b'upload {}\n{}'.format(len(blob), blob))
    buff = s.recv(FILEMAX)
    if 'ok' in buff:
        return 'Needle {} has been created'.format(buff.split()[-1])
    else:
        return 'Unknown error'

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Haystack front end')
    parser.add_argument('--dirip', help='the directory IP address',required=True)
    parser.add_argument('--dirport', type=int, help='the directory port number', required=True)
    parser.add_argument('--cacheip', help='the cache IP address', required=True)
    parser.add_argument('--cacheport', type=int, help='the cache port number', required=True)
    parser.add_argument('--ip', help='the address for this server', required=True)
    parser.add_argument('--port', type=int, help='the port number for this server', required=True)

    args = parser.parse_args()
    dirAddr = (args.dirip, args.dirport)
    cacheAddr = (args.cacheip, args.cacheport)

    app.debug = True
    app.run(host=args.ip, port=args.port)
    #app.run(host='128.2.13.188', port=5000)
