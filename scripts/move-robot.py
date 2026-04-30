import argparse
import time
from hackMatrix.api import Client

# working robot id is 21 (in db)

def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("-id", dest="robot_entity_id", required=True)
    return parser.parse_args()


def main():
    client = Client() 
    args = parse_args()
    while(True):
        client.move(int(args.robot_entity_id), 0, 0, -10, 5)
        time.sleep(5)
        client.move(int(args.robot_entity_id), 0, 0, 10, 5)
        time.sleep(5)


if __name__ == "__main__":
    main()
