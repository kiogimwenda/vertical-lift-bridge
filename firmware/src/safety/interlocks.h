// safety/interlocks.h
// Owner: M5 Ian
// Hardware safety chain: E-stop NC, relay coil, barriers.
#pragma once
#include <stdint.h>

void interlocks_init(void);
void interlocks_evaluate(void);             // Publishes estop_active/barrier state to g_status
void interlocks_request_barriers(uint8_t angle);
void interlocks_force_safe(void);
