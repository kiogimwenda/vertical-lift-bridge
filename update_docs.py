import os
import re

files_to_update = [
    r"member_guides\M3_Cindy_Breadboard_Guide.md",
    r"member_guides\M3_Cindy_Vision_Sensors.md",
    r"member_guides\M3_Cindy_Wokwi_Simulation_Guide.md",
    r"docs\known_limitations.md",
    r"README.md"
]

def replace_in_file(filepath):
    if not os.path.exists(filepath):
        return
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    # M3_Cindy_Breadboard_Guide.md specific
    if "M3_Cindy_Breadboard_Guide.md" in filepath:
        content = re.sub(
            r"1\.\s+\*\*Ultrasonics \(Shared Trigger Design\):\*\*.*?GPIO 22\.",
            r"1. **Laser Break-Beam:**\n   - Each LDR connects directly to the ESP32 pins (GPIO 18, 19, 21, 22) with a 10k pull-down resistor to GND and the LDR connecting to 3.3V (or simply use a commercial LDR comparator module outputting a digital HIGH/LOW). Blocked = LOW.",
            content,
            flags=re.DOTALL
        )
    elif "M3_Cindy_Vision_Sensors.md" in filepath:
        content = re.sub(
            r"### 5\.2 Critical wiring detail: ECHO needs a level shifter.*?ESP32 GPIO \(PIN_USx_ECHO\)",
            r"### 5.2 Critical wiring detail: LDR Input\nEach LDR connects directly to the ESP32 pins (GPIO 18, 19, 21, 22) with a 10k pull-down resistor to GND and the LDR connecting to 3.3V (or simply use a commercial LDR comparator module outputting a digital HIGH/LOW). State that blocked = LOW.",
            content,
            flags=re.DOTALL
        )
        content = re.sub(
            r"- \*\*Trigger:\*\* Shared TRIG pin for all 4 sensors connected to ESP32 GPIO 5\.\n- \*\*Echo \(with 1k/2k voltage dividers dropping 5V to 3\.3V\):\*\* US1 \(Upstream A\) to GPIO 18, US2 \(Upstream B\) to GPIO 19, US3 \(Downstream A\) to GPIO 21, US4 \(Downstream B\) to GPIO 22\.",
            r"- Each LDR connects directly to the ESP32 pins (GPIO 18, 19, 21, 22).",
            content
        )

    # Generic
    content = content.replace("HC-SR04", "Laser Break-Beam")
    content = content.replace("ultrasonic", "laser")
    content = content.replace("Ultrasonic", "Laser")
    content = content.replace("ULTRASONIC", "LASER")
    content = content.replace("echo/trigger", "digital input")
    content = content.replace("echo", "digital input")
    content = content.replace("trigger", "digital input")
    content = content.replace("Echo", "Digital Input")
    content = content.replace("Trigger", "Digital Input")

    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(content)

for f in files_to_update:
    replace_in_file(f)
