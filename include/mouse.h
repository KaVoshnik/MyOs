#ifndef _MYOS_MOUSE_H
#define _MYOS_MOUSE_H

#include <stdint.h>

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
void mouse_handle_packet(uint8_t packet[3]);
void mouse_buffer_push_direct(mouse_event_t *event);

#endif /* _MYOS_MOUSE_H */

