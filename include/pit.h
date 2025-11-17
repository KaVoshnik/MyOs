#ifndef _MYOS_PIT_H
#define _MYOS_PIT_H

#include <stdint.h>

void pit_init(uint32_t frequency_hz);
void pit_handle_tick(void);
uint64_t pit_ticks(void);
uint32_t pit_current_frequency(void);
uint64_t pit_seconds(void);

#endif /* _MYOS_PIT_H */

