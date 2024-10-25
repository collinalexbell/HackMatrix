import time

MINUTE_IN_SECS = 60
MINUTE_IN_SECS_JK = 1 # not actually 1, but just a test (1 second)

def screen_loading(n):
    print( str(n) + " minutes ")
    time.sleep(MINUTE_IN_SECS)
    screen_loading(n-1)

print("I RESTART IN THE HACK MATRIX (negative means I'm late)")
screen_loading(10)


