from hackMatrix.api import turnKey, move
import time

# this moves the boat without a head (it is adrift, save it!)
move(34, -20,0,0, 5)
time.sleep(5)
move(34, 0,0,20, 5)
time.sleep(5)

