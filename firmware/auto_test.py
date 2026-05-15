import serial
import time
import sys
import threading

def read_from_port(ser):
    while True:
        try:
            line = ser.readline()
            if line:
                print(line.decode('utf-8', errors='ignore').strip())
                sys.stdout.flush()
        except Exception:
            break

try:
    ser = serial.Serial('COM8', 115200, timeout=1)
    
    # Correct ESP32 Reset Sequence
    ser.setDTR(False)
    ser.setRTS(False)
    time.sleep(0.1)
    ser.setDTR(False)  
    ser.setRTS(True)   
    time.sleep(0.1)
    ser.setDTR(False)
    ser.setRTS(False)  
    time.sleep(0.5)
    
    print("ESP32 Reset. Starting automatic test sequence...")
    sys.stdout.flush()
    
    thread = threading.Thread(target=read_from_port, args=(ser,))
    thread.daemon = True
    thread.start()

    # Let the board boot and settle into IDLE
    time.sleep(3)
    print("\n>>> SENDING 'r' (Operator Raise)")
    ser.write(b'r\n')
    
    # Wait for ROAD_CLEARING and counterweight sequence to finish
    time.sleep(8)
    print("\n>>> SENDING 't' (Top Limit Hit)")
    ser.write(b't\n')
    
    # Wait for RAISED_HOLD (8 seconds) + transition to LOWERING
    time.sleep(10)
    print("\n>>> SENDING 'b' (Bottom Limit Hit)")
    ser.write(b'b\n')
    
    # Wait for ROAD_OPENING to finish
    time.sleep(5)
    print("\n>>> Test Sequence Complete. Closing port.")
    ser.close()
except Exception as e:
    print(f"Error: {e}")