#ifndef _MYOS_SYSTEM_H
#define _MYOS_SYSTEM_H

void system_poweroff(void);
void system_reboot(void);
__attribute__((noreturn)) void system_halt(void);

#endif /* _MYOS_SYSTEM_H */


