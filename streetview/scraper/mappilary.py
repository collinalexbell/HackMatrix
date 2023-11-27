import requests, json
import yaml
import mapillary.interface as mly

# Load credentials from creds.yaml
with open('creds.yaml', 'r') as file:
    creds = yaml.safe_load(file)

# Extract credentials from the loaded data
access_token = creds.get('access_token')

mly.set_access_token(access_token)

latitude=45.530158055556
longitude=-122.54934638889
sequences = mly.sequences_in_bbox({
    'west': longitude - 0.0000000001,
    'east': longitude + 0.0000000001,
    'north': latitude + 0.0000000001,
    'south': latitude - 0.0000000001
})

#data = mly.get_image_close_to(latitude=45.530158055556, longitude=-122.54934638889).to_dict()

print(sequences)
