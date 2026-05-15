import serial
import time
import sys

try:
    ser = serial.Serial('COM8', 115200, timeout=1)
    
    # Reset
    ser.setDTR(False)
    ser.setRTS(False)
    time.sleep(0.1)
    ser.setDTR(False)  
    ser.setRTS(True)   
    time.sleep(0.1)
    ser.setDTR(False)
    ser.setRTS(False)  
    time.sleep(0.5)
    
    print("ESP32 Reset. Listening for 30 seconds...")
    sys.stdout.flush()
    
    t_end = time.time() + 30
    while time.time() < t_end:
        line = ser.readline()
        if line:
            print(line.decode('utf-8', errors='ignore').strip())
            sys.stdout.flush()
            
    ser.close()
except Exception as e:
    print(f"Error: {e}")