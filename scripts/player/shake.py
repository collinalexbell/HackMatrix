import hackMatrix.api as matrix
import time

time.sleep(4) # exit window mode
scale = 300
swipes = 30
for i in  range(swipes):
    mult = 1
    if i % 2 == 0:
        mult = -1
    time_to_move =0.2
    matrix.player_move((scale * 0.01* mult, 0, 0), (0, 23 * mult, 0), time_to_move)
    time.sleep(time_to_move)

