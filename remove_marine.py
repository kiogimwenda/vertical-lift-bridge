import os

files = [
    r'firmware/src/fsm/fsm_actions.cpp',
    r'firmware/src/traffic/traffic_lights.cpp',
    r'firmware/src/traffic/traffic_lights.h'
]

for filepath in files:
    with open(filepath, 'r') as f:
        lines = f.readlines()
    
    with open(filepath, 'w') as f:
        for line in lines:
            if 'traffic_lights_set_marine' not in line:
                f.write(line)
