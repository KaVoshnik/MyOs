#ifndef _MYOS_SERIAL_H
#define _MYOS_SERIAL_H

#include <stdint.h>

void serial_init(void);
void serial_write_char(char c);
void serial_write(const char *s);

#endif /* _MYOS_SERIAL_H */

