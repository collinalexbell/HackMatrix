import requests, json
import yaml
import mapillary.interface as mly

# Load credentials from creds.yaml
with open('creds.yaml', 'r') as file:
    creds = yaml.safe_load(file)

# Extract credentials from the loaded data
access_token = creds.get('access_token')
mly_key = access_token

mly.set_access_token(access_token)


def getImagesBySequence(seq):
    url = f'https://graph.mapillary.com/image_ids?access_token={mly_key}&sequence_id={seq}'

    response = requests.get(url)

    image_ids = None

    if response.status_code == 200:
        json = response.json()
        image_ids = [obj['id'] for obj in json['data']]
        return image_ids
    return []

def getImages(lat, lon, radius):
    data = mly.get_image_close_to(latitude=lat, longitude=lon, radius=radius).to_dict()
    return data

def getForestImage():
    latitude = 45.529736388889
    longitude = -122.54592805556
    radius = 1
    getImages(latitude, longitude, radius)




def getJpegUrl(image_id):
    image_url = f'https://graph.mapillary.com/{image_id}?access_token={access_token}&fields=thumb_original_url'
    response = requests.get(image_url)
    image_data = response.json()
    jpeg_url = image_data['thumb_original_url']
    return jpeg_url

def downloadJpeg(name, url):
    response = requests.get(url)

    if response.status_code == 200:  # Ensure the request was successful
        with open(name, 'wb') as file:
            file.write(response.content)
            print("GET request successful. File saved.")


def downloadForest():
    images = getImagesBySequence("COGDwQJ4QMSctKFV9Ryugw")
    print(len(images))
    for image in images:
        url = getJpegUrl(image)
        downloadJpeg(f"forestStreet/{image}.jpg", url)

def fourImageScan():
    coords = [(45.509903055556,-122.64038),
              (45.509976388889,-122.64037805556),
              (45.510048055556,-122.64038),
              (45.510118055556,-122.64038138889),
              (45.510185,-122.64038805556)]

    for coord in coords:
        print(getImages(coord[0], coord[1], 0.5))

def downloadFourImageScan():
    ids = [301321988275915, 289836162803955, 755226718506030, 2973824699519337]
    for i, id in enumerate(ids):
        url = getJpegUrl(id)
        downloadJpeg(f"fourImageScan/{i}.jpg", url)

#fourImageScan()
downloadFourImageScan()

