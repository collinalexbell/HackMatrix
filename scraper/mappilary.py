import requests, json
          
# Mapillary access token -- provide your own, replace this example
mly_key = 'MLY|0000000000000000000|9856ad9cc629a15337480e7344792d78'

seq = 'kx7r2uYeFwT6XCfvqh30VZ'

url = f'https://graph.mapillary.com/image_ids?access_token={mly_key}&sequence_id={seq}'
          
response = requests.get(url)

image_ids = None

if response.status_code == 200:
   json = response.json()
   image_ids = [obj['id'] for obj in json['data']]
   
   # make a dictionary to store each detection by image ID
   detections = {}
   for image_id in image_ids:
      dets_url = f'https://graph.mapillary.com/{image_id}/detections?access_token={mly_key}&fields=geometry,value'
      response = requests.get(dets_url)
      json = response.json()
      detections[image_id] = json['data']
