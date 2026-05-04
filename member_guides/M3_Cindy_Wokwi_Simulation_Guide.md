# Wokwi Simulation Guide for M3: Vision & Sensors (Cindy)

This guide provides a step-by-step walkthrough for simulating the components owned by M3 (Cindy) using **Wokwi**, an online electronics simulator. It focuses on the **Quad HC-SR04 Ultrasonic Array** and the **ESP32-CAM Vision Link**, remaining strictly loyal to the Group 7 system architecture.

---

## Understanding Wokwi's Capabilities and Limitations

Before we begin, it's vital to understand what Wokwi *can* and *cannot* do regarding M3's specific hardware:

1. **HC-SR04 Ultrasonics**: Wokwi natively supports HC-SR04 sensors. You can use an interactive slider during the simulation to change the distance, making it perfect for testing the direction inference logic (approaching vs. leaving) across all four sensors.
2. **ESP32-CAM (OV2640)**: Wokwi **does not** simulate physical camera sensors or image processing algorithms (like frame-differencing). 
   * **The Workaround:** Since the main ESP32 only cares about the data coming *out* of the ESP32-CAM over UART2 (the JSON strings), we will simulate the camera by injecting mock JSON data into the UART RX pin of the main ESP32.

---

## Step 1: Initializing the Wokwi Project

1. Open your browser and go to [wokwi.com](https://wokwi.com).
2. Scroll down to "Start from Scratch" and select **ESP32**.
3. You will see an empty breadboard with an ESP32 module and a default `sketch.ino`.
4. Ensure the environment is set to use FreeRTOS and Arduino (the default).

---

## Step 2: Simulating the Quad HC-SR04 Ultrasonic Array

M3 is responsible for the direction inference logic using two pairs of sensors: an **Upstream Pair (US1 + US2)** and a **Downstream Pair (US3 + US4)**. Within each pair, the sensors are spaced 3 cm apart to determine if a vessel is approaching or leaving the bridge.

### 2.1 Adding the Hardware in Wokwi
1. Click the **"+" (Add Part)** button at the top of the simulation window.
2. Search for **"HC-SR04"** and add **four** of them to the workspace.
3. Arrange them in two pairs and label them mentally:
   * **Upstream:** US1 (Beam A) and US2 (Beam B)
   * **Downstream:** US3 (Beam A) and US4 (Beam B)

### 2.2 Wiring the Sensors
Refer to `firmware/src/pin_config.h` in your actual project for the exact pins. Wire them in Wokwi as follows:

* **US1 (Upstream Beam A):**
  * `VCC` -> `5V`, `GND` -> `GND`
  * `TRIG` -> `GPIO 5` *(Note: using GPIO 4/5 as free pins for sim since 5 is shared)*
  * `ECHO` -> `GPIO 18`
* **US2 (Upstream Beam B):**
  * `VCC` -> `5V`, `GND` -> `GND`
  * `TRIG` -> `GPIO 19`
  * `ECHO` -> `GPIO 21`
* **US3 (Downstream Beam A):**
  * `VCC` -> `5V`, `GND` -> `GND`
  * `TRIG` -> `GPIO 22`
  * `ECHO` -> `GPIO 23`
* **US4 (Downstream Beam B):**
  * `VCC` -> `5V`, `GND` -> `GND`
  * `TRIG` -> `GPIO 1` *(TX0)*
  * `ECHO` -> `GPIO 3` *(RX0)*

### 2.3 Implementing the Simulation Code
Replace the code in `sketch.ino` with the mock sensor task. This simulates the `task_sensors` from the main project, handling all four sensors and combining their logic.

```cpp
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Mock pins based on pin_config.h
#define US1_TRIG 5
#define US1_ECHO 18
#define US2_TRIG 19
#define US2_ECHO 21
#define US3_TRIG 22
#define US3_ECHO 23
#define US4_TRIG 1
#define US4_ECHO 3

// Mock status
float dist_us1 = 0, dist_us2 = 0;
float dist_us3 = 0, dist_us4 = 0;

long readUltrasonic(int trigPin, int echoPin) {
  // If we are using GPIO 1/3 (UART0) we must be careful with Serial,
  // but in Wokwi simulation we can bitbang it.
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
  pinMode(US1_TRIG, OUTPUT); pinMode(US1_ECHO, INPUT);
  pinMode(US2_TRIG, OUTPUT); pinMode(US2_ECHO, INPUT);
  pinMode(US3_TRIG, OUTPUT); pinMode(US3_ECHO, INPUT);
  pinMode(US4_TRIG, OUTPUT); pinMode(US4_ECHO, INPUT);

  for (;;) {
    // Note: To avoid cross-talk, sensors are pulsed sequentially
    dist_us1 = readUltrasonic(US1_TRIG, US1_ECHO);
    dist_us2 = readUltrasonic(US2_TRIG, US2_ECHO);
    dist_us3 = readUltrasonic(US3_TRIG, US3_ECHO);
    dist_us4 = readUltrasonic(US4_TRIG, US4_ECHO);

    bool approach = false;
    bool leave = false;

    // Upstream Direction Inference Mock
    if (dist_us1 < 80 && dist_us2 > 80) {
      Serial.println("[sensors] Upstream: Vessel triggered Beam A: APPROACHING bridge");
      approach = true;
    } else if (dist_us2 < 80 && dist_us1 > 80) {
      Serial.println("[sensors] Upstream: Vessel triggered Beam B: LEAVING bridge");
      leave = true;
    }

    // Downstream Direction Inference Mock
    if (dist_us4 < 80 && dist_us3 > 80) {
      Serial.println("[sensors] Downstream: Vessel triggered Beam B: APPROACHING bridge");
      approach = true;
    } else if (dist_us3 < 80 && dist_us4 > 80) {
      Serial.println("[sensors] Downstream: Vessel triggered Beam A: LEAVING bridge");
      leave = true;
    }

    if (approach && !leave) {
       // In real code, this helps trigger EVT_VEHICLE_DETECTED
       // Serial.println("--> GLOBAL STATE: VESSEL APPROACHING");
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

### 2.4 Testing the Quad Setup
1. Click the **Play (Run)** button.
2. **Test Upstream Approaching:** Click on the US1 sensor (Beam A) and drag the slider below 80cm. The console will print "Upstream: ... APPROACHING".
3. **Test Upstream Leaving:** Drag US1 back above 80cm. Drag US2 (Beam B) below 80cm. The console prints "Upstream: ... LEAVING".
4. **Test Downstream Approaching:** Drag the US4 sensor (outermost downstream beam) below 80cm to simulate a boat coming from the other side.
5. **Test Downstream Leaving:** Drag US3 (innermost downstream beam) below 80cm to simulate the boat exiting the bridge zone.

---

## Step 3: Simulating the ESP32-CAM UART Link

Since Wokwi can't process images, we will simulate `task_vision` by injecting data directly into the ESP32's `UART2` port, mimicking the exact JSON string the real ESP32-CAM sends.

### 3.1 Wiring the Mock UART
We don't need a physical second ESP32 in Wokwi. We will use the built-in `Serial` monitor to simulate `UART2` for the sake of the exercise, allowing you to type the JSON payloads manually.

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

// ... (Merge this into the setup() and loop() from the Ultrasonic code above) ...
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
1. Validate the math and timing for the complete quad `task_sensors` array (delay between Beam A and Beam B breaking on both the upstream and downstream sides).
2. Validate the JSON parsing logic, memory allocation (`StaticJsonDocument` sizing), and heartbeat timeout logic for the `task_vision` module without needing physical hardware.
3. Safely intentionally trigger edge cases (like partial JSON strings or bouncing ultrasonic readings) to ensure the firmware handles them gracefully.