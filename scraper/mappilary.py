import requests, json
import yaml


# Load credentials from creds.yaml
with open('creds.yaml', 'r') as file:
    creds = yaml.safe_load(file)

# Extract credentials from the loaded data
access_token = creds.get('access_token')

# Mapillary access token -- provide your own, replace this example
mly_key = access_token

seq = 'kx7r2uYeFwT6XCfvqh30VZ'

url = f'https://graph.mapillary.com/image_ids?access_token={mly_key}&sequence_id={seq}'

response = requests.get(url)

image_ids = None

if response.status_code == 200:
    json = response.json()
    image_ids = [obj['id'] for obj in json['data']]
    print(image_ids)
