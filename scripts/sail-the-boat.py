from hackMatrix.api import turnKey, move, player_move
import argparse
import time

def forward_sequence():
    """Execute the forward movement sequence"""
    player_move((60, 25, 75), (0, 0, 0), 4)
    time.sleep(5)
    move(34, -20, 0, 0, 5)
    time.sleep(5)
    move(34, 0, 0, 20, 5)
    time.sleep(5)
    print("Forward sequence completed")

def return_sequence():
    """Execute the safe return sequence"""
    # First move down to safe water level
    move(34, 0, 0, -20, 5)
    time.sleep(5)
    # Then return to original position through safe waters
    move(34, 20, 0, 0, 5)
    time.sleep(5)
    print("Safe return sequence completed")

def execute_moves(undo=False, run_and_undo=False):
    """
    Execute boat movement sequence based on operation mode
    
    Args:
        undo (bool): Execute only the return sequence
        run_and_undo (bool): Execute forward sequence followed by return
    """
    try:
        if run_and_undo:
            forward_sequence()
            print("Initiating return sequence...")
            return_sequence()
        elif undo:
            return_sequence()
        else:
            forward_sequence()
    except Exception as e:
        print(f"EMERGENCY: Navigation error: {e}")
        try:
            move(34, 0, 0, 0, 1)  # Attempt to hold position
            print("Emergency position hold engaged")
        except:
            print("CRITICAL: Unable to stabilize position!")
        raise

def main():
    parser = argparse.ArgumentParser(description='Control boat movements with multiple operation modes')
    parser.add_argument('-u', '--undo', action='store_true', 
                        help='Execute only the return sequence')
    parser.add_argument('-r', '--run-and-undo', action='store_true',
                        help='Execute forward sequence followed by return')
    
    args = parser.parse_args()
    
    if args.undo and args.run_and_undo:
        print("Error: Cannot specify both --undo and --run-and-undo")
        return 1
    
    try:
        execute_moves(undo=args.undo, run_and_undo=args.run_and_undo)
        return 0
    except Exception:
        return 1

if __name__ == "__main__":
    exit(main())
