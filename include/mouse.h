#ifndef _MYOS_MOUSE_H
#define _MYOS_MOUSE_H

#include <stdint.h>
#include <stddef.h>

#ifndef ENABLE_MOUSE_DRIVER
#define ENABLE_MOUSE_DRIVER 0
#endif

#define MOUSE_EVENT_SCROLL_UP    1
#define MOUSE_EVENT_SCROLL_DOWN  2
#define MOUSE_EVENT_BUTTON_LEFT  4
#define MOUSE_EVENT_BUTTON_RIGHT 8
#define MOUSE_EVENT_BUTTON_MIDDLE 16

typedef struct {
    int x;
    int y;
    int buttons;
    int scroll;
} mouse_event_t;

void mouse_init(void);
int mouse_is_available(void);
int mouse_get_event(mouse_event_t *event);
int mouse_try_get_event(mouse_event_t *event);
void mouse_buffer_push_direct(mouse_event_t *event);
size_t mouse_packet_length(void);

#endif /* _MYOS_MOUSE_H */

