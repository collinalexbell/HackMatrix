#!/usr/bin/env python3
"""
Simple voxel demo: add a line of voxels, then remove them.
Uses the HackMatrix API on tcp://127.0.0.1:4455 (default Client address).
"""
import time
from hackMatrix.api import Client

client = Client()  # defaults to tcp://127.0.0.1:4455

positions = [(x, 6, 6) for x in range(-6, 8,2)]

print("Adding voxels...")
client.add_voxels(positions, replace=True, size=2.0)
time.sleep(2)

print("Clearing voxels...")
client.add_voxels([], replace=True, size=2.0)

print("Done.")
