#ifndef _MYOS_PIT_H
#define _MYOS_PIT_H

#include <stdint.h>

void pit_init(uint32_t frequency_hz);
void pit_handle_tick(void);
uint64_t pit_ticks(void);

#endif /* _MYOS_PIT_H */

