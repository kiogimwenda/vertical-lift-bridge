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
    time.sleep(1.0)
    
    print("ESP32 Reset. Waiting for boot...")
    
    # Read boot logs
    t_end = time.time() + 3
    while time.time() < t_end:
        line = ser.readline()
        if line:
            print(line.decode('utf-8', errors='ignore').strip())
            
    # Send commands
    for cmd in ['0', '1', '2', '3', '4']:
        print(f"\n>>> SENDING '{cmd}'")
        ser.write(cmd.encode('utf-8'))
        ser.flush()
        
        t_cmd = time.time() + 2
        while time.time() < t_cmd:
            line = ser.readline()
            if line:
                print(line.decode('utf-8', errors='ignore').strip())

    ser.close()
except Exception as e:
    print(f"Error: {e}")
