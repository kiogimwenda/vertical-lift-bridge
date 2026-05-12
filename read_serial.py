import serial
import time
import sys

try:
    # Open serial port
    ser = serial.Serial('COM8', 115200, timeout=1)
    
    # Correct ESP32 Reset Sequence
    ser.setDTR(False)
    ser.setRTS(False)
    time.sleep(0.1)
    ser.setDTR(False)  
    ser.setRTS(True)   # EN = LOW, IO0 = HIGH
    time.sleep(0.1)
    ser.setDTR(False)
    ser.setRTS(False)  # EN = HIGH, IO0 = HIGH (Boot normally)
    time.sleep(0.1)
    
    print("ESP32 Reset. Listening to serial output for 10 seconds...")
    sys.stdout.flush()
    
    t_end = time.time() + 10
    while time.time() < t_end:
        line = ser.readline()
        if line:
            print(line.decode('utf-8', errors='ignore').strip())
            sys.stdout.flush()
            
    ser.close()
except Exception as e:
    print(f"Error: {e}")