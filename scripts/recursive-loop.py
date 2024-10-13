
def loop(n, do):
    if(n>0):
        do(n)
        loop(n-1, do)

def print_n(n):
    print(n)

def print_5():
    loop(5, print_n)

print_5()
