import time
import pyaudio
import numpy as np
import zmq
from scipy.fft import fft
from hackMatrix.api import add_cube,clear_box, add_cubes, Cube


# Initialize PyAudio to capture audio
audio = pyaudio.PyAudio()

device_count = audio.get_device_count()
print("Available audio devices:")
for i in range(device_count):
    device_info = audio.get_device_info_by_index(i)
    print(f"Device {i}: {device_info['name']}")

# Set up parameters for audio capture
sample_rate = 44100 # Sample rate in Hz
chunk_size = int(44100 * 0.005) # Number of audio frames per chunk
format = pyaudio.paInt16

# Initialize the graphics API through ZeroMQ
#context = zmq.Context()
#socket = context.socket(zmq.PUB)
#socket.bind("tcp://127.0.0.1:5555")  # Replace with your API details

# Create a PyAudio stream
stream = audio.open(format=format, channels=1, rate=sample_rate, input=True, frames_per_buffer=chunk_size)

last_time = time.time()
time_interval = 1/2
while True:
    try:
        data = stream.read(chunk_size)
        audio_data = np.frombuffer(data, dtype=np.int16)
        # Perform audio analysis and extract features (e.g., amplitude)
        freqs = fft(audio_data, n=40)
        # Send feature data to the graphics API using ZMQ
        #socket.send_string(str(feature))
        height = 40
        freqs = np.abs(freqs) / 32768.0 * height
        freqs = np.minimum(freqs, height)
        cubes = []
        for x, freq in enumerate(freqs[1:(int(len(freqs)/2))]):
            for y in range(int(freq)+1):
                cubes.append(Cube(x,y+20,80,1))
            for y in range(int(freq)+1, height):
                cubes.append(Cube(x,y+20,80,-1))
        if time.time() - last_time > time_interval:
            add_cubes(cubes)
            last_time = time.time()

    except KeyboardInterrupt:
        break

# Cleanup
stream.stop_stream()
stream.close()
audio.terminate()
