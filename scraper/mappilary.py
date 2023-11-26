import requests, json
import yaml
import mapillary.interface as mly

# Load credentials from creds.yaml
with open('creds.yaml', 'r') as file:
    creds = yaml.safe_load(file)

# Extract credentials from the loaded data
access_token = creds.get('access_token')

mly.set_access_token(access_token)

data = mly.get_image_close_to(latitude=45.530158055556, longitude=-122.54934638889).to_dict()

print(data)
