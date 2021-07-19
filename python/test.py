import requests
from time import sleep
while True:
    r =requests.get('http://127.0.0.1:60000/json.json')
    print(r.json())