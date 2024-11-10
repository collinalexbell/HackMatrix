import pygame
from hackMatrix.api import turnKey, move, player_move
import time

# Initialize Pygame for keyboard input
pygame.init()
screen = pygame.display.set_mode((400, 300))  # Minimal window for input capture
pygame.display.set_caption("WASD Control Interface")

def handle_movement(key_pressed):
    """
    Translate WASD key presses into HackMatrix API movements
    
    Parameters:
        key_pressed: Dictionary of pressed keys
    """
    try:
        # Movement parameters
        move_distance = 10  # Standard movement distance
        move_time = 2      # Time for movement execution
        
        # W - Forward movement
        if key_pressed[pygame.K_w]:
            move(34, move_distance, 0, 0, move_time)
            print("Moving forward")
            
        # S - Backward movement
        if key_pressed[pygame.K_s]:
            move(34, -move_distance, 0, 0, move_time)
            print("Moving backward")
            
        # A - Left movement
        if key_pressed[pygame.K_a]:
            move(34, 0, -move_distance, 0, move_time)
            print("Moving left")
            
        # D - Right movement
        if key_pressed[pygame.K_d]:
            move(34, 0, move_distance, 0, move_time)
            print("Moving right")
            
    except Exception as e:
        print(f"Movement error: {e}")
        emergency_stop()

def emergency_stop():
    """Emergency stop function"""
    try:
        move(34, 0, 0, 0, 1)  # Immediate stop
        print("Emergency stop engaged")
    except Exception as e:
        print(f"Emergency stop failed: {e}")

def main():
    running = True
    last_movement_time = 0
    movement_cooldown = 0.1  # Seconds between movements
    
    print("WASD Control Interface Active")
    print("Controls:")
    print("W - Forward")
    print("S - Backward")
    print("A - Left")
    print("D - Right")
    print("ESC - Exit")
    
    try:
        while running:
            current_time = time.time()
            
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    running = False
                elif event.type == pygame.KEYDOWN:
                    if event.key == pygame.K_ESCAPE:
                        running = False
            
            # Handle movement if enough time has passed since last movement
            if current_time - last_movement_time >= movement_cooldown:
                keys = pygame.key.get_pressed()
                if any([keys[pygame.K_w], keys[pygame.K_a], keys[pygame.K_s], keys[pygame.K_d]]):
                    handle_movement(keys)
                    last_movement_time = current_time
            
            # Small delay to prevent excessive CPU usage
            pygame.time.delay(50)
            
    except KeyboardInterrupt:
        print("\nController stopped by user")
    except Exception as e:
        print(f"Controller error: {e}")
    finally:
        emergency_stop()
        pygame.quit()

if __name__ == "__main__":
    main()
