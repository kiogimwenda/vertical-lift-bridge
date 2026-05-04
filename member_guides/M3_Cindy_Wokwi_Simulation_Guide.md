# Wokwi Simulation Guide for M3: Vision & Sensors (Cindy)

This guide provides a step-by-step walkthrough for simulating the components owned by M3 (Cindy) using **Wokwi**, an online electronics simulator. It focuses on the **Dual HC-SR04 Ultrasonic Array** and the **ESP32-CAM Vision Link**, remaining strictly loyal to the Group 7 system architecture.

---

## Understanding Wokwi's Capabilities and Limitations

Before we begin, it's vital to understand what Wokwi *can* and *cannot* do regarding M3's specific hardware:

1. **HC-SR04 Ultrasonics**: Wokwi natively supports HC-SR04 sensors. You can use an interactive slider during the simulation to change the distance, making it perfect for testing the direction inference logic (approaching vs. leaving).
2. **ESP32-CAM (OV2640)**: Wokwi **does not** simulate physical camera sensors or image processing algorithms (like frame-differencing). 
   * **The Workaround:** Since the main ESP32 only cares about the data coming *out* of the ESP32-CAM over UART2 (the JSON strings), we will simulate the camera by injecting mock JSON data into the UART RX pin of the main ESP32.

---

## Step 1: Initializing the Wokwi Project

1. Open your browser and go to [wokwi.com](https://wokwi.com).
2. Scroll down to "Start from Scratch" and select **ESP32**.
3. You will see an empty breadboard with an ESP32 module and a default `sketch.ino`.
4. Ensure the environment is set to use FreeRTOS and Arduino (the default).

---

## Step 2: Simulating the HC-SR04 Ultrasonic Array

M3 is responsible for the direction inference logic using two pairs of sensors (Upstream and Downstream). For this simulation, we will focus on one pair (US1 and US2) to prove the `DIR_APPROACHING` / `DIR_LEAVING` logic.

### 2.1 Adding the Hardware in Wokwi
1. Click the **"+" (Add Part)** button at the top of the simulation window.
2. Search for **"HC-SR04"** and add two of them to the workspace.
3. Label them mentally as **US1 (Beam A)** and **US2 (Beam B)**.

### 2.2 Wiring the Sensors
Refer to `firmware/src/pin_config.h` in your actual project for the exact pins, but for this simulation, wire them as follows:

* **US1 (Beam A):**
  * `VCC` -> ESP32 `5V` (or `VIN`)
  * `GND` -> ESP32 `GND`
  * `TRIG` -> ESP32 `GPIO 4`
  * `ECHO` -> ESP32 `GPIO 5` (In the real world, use a voltage divider here; in Wokwi, direct connection is fine).
* **US2 (Beam B):**
  * `VCC` -> ESP32 `5V`
  * `GND` -> ESP32 `GND`
  * `TRIG` -> ESP32 `GPIO 18`
  * `ECHO` -> ESP32 `GPIO 19`

### 2.3 Implementing the Simulation Code
Replace the code in `sketch.ino` with the mock sensor task. This simulates the `task_sensors` from the main project.

```cpp
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Mock pins
#define US1_TRIG 4
#define US1_ECHO 5
#define US2_TRIG 18
#define US2_ECHO 19

// Mock status
float dist_us1 = 0;
float dist_us2 = 0;

long readUltrasonic(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, 30000); // 30ms timeout
  if (duration == 0) return 999; // No echo
  return duration * 0.034 / 2;
}

void task_sensors(void *pvParameters) {
  pinMode(US1_TRIG, OUTPUT);
  pinMode(US1_ECHO, INPUT);
  pinMode(US2_TRIG, OUTPUT);
  pinMode(US2_ECHO, INPUT);

  for (;;) {
    dist_us1 = readUltrasonic(US1_TRIG, US1_ECHO);
    dist_us2 = readUltrasonic(US2_TRIG, US2_ECHO);

    // Basic Direction Inference Mock
    if (dist_us1 < 80 && dist_us2 > 80) {
      Serial.println("[sensors] Vessel triggered Beam A: APPROACHING");
    } else if (dist_us2 < 80 && dist_us1 > 80) {
      Serial.println("[sensors] Vessel triggered Beam B: LEAVING");
    }

    vTaskDelay(pdMS_TO_TICKS(50)); // 20 Hz tick
  }
}

void setup() {
  Serial.begin(115200);
  xTaskCreate(task_sensors, "sensors", 2048, NULL, 3, NULL);
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
```

### 2.4 Testing the Sensors
1. Click the **Play (Run)** button.
2. Click on the first HC-SR04 sensor in the diagram. A slider will appear.
3. Drag the slider below 80cm. Observe the Serial Terminal printing "APPROACHING".
4. Drag it back up, then drag the second sensor's slider below 80cm to see "LEAVING".

---

## Step 3: Simulating the ESP32-CAM UART Link

Since Wokwi can't process images, we will simulate `task_vision` by injecting data directly into the ESP32's `UART2` port, mimicking the exact JSON string the real ESP32-CAM sends.

### 3.1 Wiring the Mock UART
We don't need a physical second ESP32 in Wokwi. We will use the built-in `Serial` monitor to simulate `UART2` for the sake of the exercise, allowing you to type the JSON payloads manually.

*(Note: In the physical rig, `Serial` is UART0 for USB, and the CAM is on UART2 (`GPIO 16/17`). In Wokwi, we'll map UART2 logic to the main Serial terminal so you can easily type the inputs).*

### 3.2 Implementing the Vision Link Parser
Update the `sketch.ino` to include the `task_vision` JSON parsing logic. This is highly loyal to the `vision_link.cpp` logic.

```cpp
#include <Arduino.h>
#include <ArduinoJson.h> // You must add this library in Wokwi!
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Mock Vision Status (matching system_types.h)
struct {
  bool vehicle_present;
  uint8_t confidence;
  uint32_t last_seen_ms;
} vision;

void task_vision(void *pvParameters) {
  // Simulating UART2 on Serial for easy typing in Wokwi
  Serial.println("Type JSON like: {\"v\":1,\"c\":95} to simulate ESP32-CAM");
  
  String buffer = "";
  
  for (;;) {
    while (Serial.available() > 0) {
      char c = Serial.read();
      if (c == '\n') {
        // Parse the JSON line
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, buffer);
        
        if (!error) {
          vision.vehicle_present = doc["v"] == 1;
          vision.confidence = doc["c"];
          vision.last_seen_ms = millis();
          
          Serial.printf("[vision] Parsed: Vehicle=%d, Confidence=%d%%\n", 
                        vision.vehicle_present, vision.confidence);
                        
          // In the real system, this triggers EVT_VEHICLE_DETECTED
        } else {
          Serial.println("[vision] JSON Parse Error");
        }
        buffer = ""; // clear buffer
      } else {
        buffer += c;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void setup() {
  Serial.begin(115200);
  // Create vision task
  xTaskCreate(task_vision, "vision", 4096, NULL, 3, NULL);
}

void loop() {
  // Watchdog timeout check simulation
  if (millis() - vision.last_seen_ms > 2000 && vision.last_seen_ms != 0) {
     Serial.println("[fault] VISION_LINK_LOST (Heartbeat timeout!)");
     vision.last_seen_ms = 0; // Prevent spam
  }
  vTaskDelay(pdMS_TO_TICKS(1000));
}
```

### 3.3 Adding the ArduinoJson Library in Wokwi
1. Click on the **"Library Manager"** tab (the books icon on the left sidebar).
2. Click **"+"**.
3. Search for **"ArduinoJson"** (by Benoit Blanchon) and add it.

### 3.4 Testing the Vision Link
1. Click **Play**.
2. Click in the Serial Terminal window at the bottom.
3. Type the following exactly and press Enter:
   `{"v":1,"c":98}`
4. The terminal will output `[vision] Parsed: Vehicle=1, Confidence=98%`.
5. Wait for 2 seconds without typing anything. The main loop will output `[fault] VISION_LINK_LOST (Heartbeat timeout!)`, accurately simulating the system's safety watchdog detecting that the ESP32-CAM has crashed or disconnected.

---

## Summary of the Wokwi Workflow

By using Wokwi in this manner, M3 can:
1. Validate the math and timing for the `task_sensors` array (delay between Beam A and Beam B breaking).
2. Validate the JSON parsing logic, memory allocation (`StaticJsonDocument` sizing), and heartbeat timeout logic for the `task_vision` module without needing physical hardware.
3. Safely intentionally trigger edge cases (like partial JSON strings or bouncing ultrasonic readings) to ensure the firmware handles them gracefully.