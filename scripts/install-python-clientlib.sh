#!/bin/bash

# IMPORTANT: TO BE EXECUTED FROM <HackMatrixRoot>

# Install python hackMatrix lib
python -m venv hackmatrix_python
cd hackmatrix_python
source bin/activate
cd client_libs/python
pip install .
cd ../..

# Testing
python scripts/player-move.py
