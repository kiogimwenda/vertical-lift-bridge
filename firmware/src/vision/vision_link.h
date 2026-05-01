// vision/vision_link.h
// UART2 link to ESP32-CAM companion. JSON line-protocol.
// Owner: M3 Cindy
#pragma once
#include "../system_types.h"

void vision_link_init(void);
void vision_link_tick(void);
VisionStatus_t vision_link_get(void);
