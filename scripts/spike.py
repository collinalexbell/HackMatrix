from hackMatrix.api import turnKey, move
import time

try:
    SPIKE_ID = 39
    spikeDirection = 1
    moveTime = 1.5
    repeat = 2
    while True:
        for x in range(repeat):
            move(SPIKE_ID, 20 * spikeDirection, 0 ,0, moveTime)
            spikeDirection *= -1
            time.sleep(moveTime)
        for y in range(repeat):
            move(SPIKE_ID, 0, 20 * spikeDirection ,0, moveTime)
            spikeDirection *= -1
            time.sleep(moveTime)
except e:
    print(e)
