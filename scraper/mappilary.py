import requests, json
import yaml


# Load credentials from creds.yaml
with open('creds.yaml', 'r') as file:
    creds = yaml.safe_load(file)

# Extract credentials from the loaded data
access_token = creds.get('access_token')

# Mapillary access token -- provide your own, replace this example
mly_key = access_token

image_id = 520273309103543
minLat = 45.530158055546
maxLat = 45.530158055576
minLon = -122.54934638899
maxLon = -122.54934638887

#url = f'https://graph.mapillary.com/images?access_token={access_token}&lat=45.530158055556&lng=-122.54934638889&z=17'

url = f'https://graph.mapillary.com/images?access_token={access_token}&bbox={minLon},{minLat},{maxLon},{maxLat}'

response = requests.get(url)

print(response)
if response.status_code == 200:
    print(response.text)
    json = response.json()
    print(json)

