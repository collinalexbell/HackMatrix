import hackMatrix.api as matrix
import player.shake as shake
import time

matrix.unfocus_app()
scale = 300 
burst_margin_t = 3
matrix.player_move((scale/5, 0, 0), (0, 180, 0), 7)
time.sleep(7+burst_margin_t)
matrix.player_move((0, scale * 0.6, 0), (0, 180, 0), 3)
time.sleep(10)

swipes = 7 
for i in range(swipes):
    mult = 1
    if i % 2 == 0:
        mult = -1
    time_to_move = 2.5
    matrix.player_move((scale * mult, 0, 0), (0, 23 * mult, 0), time_to_move)
    time.sleep(time_to_move)

time.sleep(5)

# teleport to a window on the ship
# super + e

shake.shake()
