#!/usr/bin/env python3
"""
Simple voxel demo: add a line of voxels, then remove them.
Uses the HackMatrix API on tcp://127.0.0.1:4455 (default Client address).
"""
import time
from hackMatrix.api import Client

client = Client("tcp://127.0.0.1:3345")  # defaults to VOXEL_API_ADDRESS or tcp://127.0.0.1:4455

vert_lines = []
for i in range (0,2):
        vert_lines.append([(x, 20-(i*40), 6) for x in range (-20,20, 5)])
        vert_lines.append([(x, 20-(i*40), 6) for x in range (-20,20, 5)])

positions = [(x, 6, 6) for x in range(-6, 8,2)]

print("Adding voxels...")
client.add_voxels(positions, replace=True, size=2.0)
for line in vert_lines: 
    client.add_voxels(line, replace=False, size=2.0)

time.sleep(7)
print("Requesting clear on left half (x < 0)...")
action_id = client.clear_voxels((-25, 0), (-30, 30), (0, 12))
input(f"Press enter to confirm clear (action {action_id})")
client.confirm_action(action_id)

print("Done.")
