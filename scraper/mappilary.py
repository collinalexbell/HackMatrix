import requests, json

# Define the necessary details
client_id = 'your_client_id'
client_secret = 'your_client_secret'
token_url = 'https://example.com/oauth/token'  # Replace with your token URL

token_params = {
    'grant_type': 'client_credentials',  # Adjust based on the OAuth flow you are using
    'client_id': client_id,
    'client_secret': client_secret
}

# Other parameters required by the token endpoint
response = requests.post(token_url, data=token_params)

# Check if the request was successful
if response.status_code == 200:
    # Access token received
    access_token = response.json().get('access_token')
    print(f"Access Token: {access_token}")

    # Mapillary access token -- provide your own, replace this example
    mly_key = access_token

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
