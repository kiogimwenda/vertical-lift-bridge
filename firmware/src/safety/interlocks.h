// safety/interlocks.h
// Owner: M5 Ian
// Hardware safety chain: E-stop NC, relay coil, barriers.
#pragma once
#include <stdint.h>

void interlocks_init(void);
void interlocks_evaluate(void);
void interlocks_request_barriers(uint8_t angle);
void interlocks_force_safe(void);
bool interlocks_estop_active(void);
