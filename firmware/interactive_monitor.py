import serial
import time
import sys
import threading

def read_from_port(ser):
    while True:
        try:
            line = ser.readline()
            if line:
                print("\r" + line.decode('utf-8', errors='ignore').strip() + " " * 20)
                print("Enter command (0-4 to test LEDs, q to quit): ", end="", flush=True)
        except Exception:
            break

try:
    ser = serial.Serial('COM8', 115200, timeout=0.1)
    
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
    
    print("ESP32 Reset. Starting Interactive Mode...")
    print("Type a number (0, 1, 2, 3, or 4) and press Enter.")
    
    thread = threading.Thread(target=read_from_port, args=(ser,))
    thread.daemon = True
    thread.start()

    while True:
        cmd = input("Enter command (0-4 to test LEDs, q to quit): ").strip()
        if cmd == 'q':
            break
        if cmd in ['0', '1', '2', '3', '4']:
            # Send the character without a newline, as the firmware expects
            ser.write(cmd.encode('utf-8'))
            ser.flush()
        else:
            print("Invalid command. Use 0, 1, 2, 3, or 4.")
            
    print("Closing port.")
    ser.close()
except Exception as e:
    print(f"Error: {e}")
