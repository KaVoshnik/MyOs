#include "mouse.h"
#include "io.h"  // Используем ваши inline функции для работы с портами
#include <string.h>  // Для memset

// Глобальное состояние мыши
static mouse_state_t mouse_state = {0};

// Отправить команду мыши
static void mouse_write(uint8_t command) {
    // Ждем, пока контроллер будет готов принять команду
    mouse_wait(1);
    // Отправляем байт команды в порт данных
    outb(PS2_DATA_PORT, command);
}

// Прочитать байт от мыши
static uint8_t mouse_read(void) {
    // Ждем, пока появятся данные для чтения
    mouse_wait(0);
    // Читаем байт из порта данных
    return inb(PS2_DATA_PORT);
}

// Ожидание готовности контроллера
void mouse_wait(uint8_t type) {
    // type=0 - ждем данные для чтения
    // type=1 - ждем возможность записи
    uint32_t timeout = 100000;
    while (timeout--) {
        uint8_t status = inb(PS2_STATUS_PORT);
        
        if (type == 0) {
            // Проверяем, есть ли данные для чтения
            if (status & STATUS_OUTPUT_FULL) {
                return;
            }
        } else {
            // Проверяем, можно ли писать
            if (!(status & STATUS_INPUT_FULL)) {
                return;
            }
        }
    }
}

// Включить колесико мыши (Scroll Wheel)
static void enable_scroll_wheel(void) {
    // Последовательность команд для включения колесика
    mouse_write(MOUSE_CMD_SET_SAMPLE_RATE);
    mouse_read(); // Ждем ACK
    mouse_write(200);
    mouse_read(); // ACK
    
    mouse_write(MOUSE_CMD_SET_SAMPLE_RATE);
    mouse_read(); // ACK
    mouse_write(100);
    mouse_read(); // ACK
    
    mouse_write(MOUSE_CMD_SET_SAMPLE_RATE);
    mouse_read(); // ACK
    mouse_write(80);
    mouse_read(); // ACK
    
    // Запросим Device ID для проверки поддержки колесика
    mouse_write(MOUSE_CMD_GET_DEVICE_ID);
    mouse_read(); // ACK
    uint8_t device_id = mouse_read();
    
    if (device_id == 0x03) {
        mouse_state.packet_size = 4; // Мышь с колесиком
    } else {
        mouse_state.packet_size = 3; // Старая мышь без колесика
    }
}

// Инициализация мыши
void mouse_init(void) {
    uint8_t status;
    
    // Инициализируем состояние
    memset(&mouse_state, 0, sizeof(mouse_state));
    mouse_state.packet_size = 3;
    
    // Включить вспомогательное устройство (мышь)
    mouse_wait(1);
    outb(PS2_COMMAND_PORT, 0xA8);
    
    // Включить прерывания от мыши
    mouse_wait(1);
    outb(PS2_COMMAND_PORT, 0x20);
    mouse_wait(0);
    status = inb(PS2_DATA_PORT);
    status |= 0x02; // Включить бит прерывания мыши
    status |= 0x01; // Включить бит прерывания клавиатуры
    mouse_wait(1);
    outb(PS2_COMMAND_PORT, 0x60);
    mouse_wait(1);
    outb(PS2_DATA_PORT, status);
    
    // Установить режим мыши
    mouse_write(MOUSE_CMD_SET_REMOTE_MODE);
    mouse_read(); // ACK
    
    // Включить колесико
    enable_scroll_wheel();
    
    // Включить мышь
    mouse_write(MOUSE_CMD_ENABLE);
    mouse_read(); // ACK
    
    mouse_state.initialized = 1;
}

// Обработчик прерывания мыши
void mouse_handler(void) {
    static uint8_t mouse_cycle = 0;
    static int8_t mouse_packet[4];
    
    // Читаем байт из порта данных
    uint8_t data = inb(PS2_DATA_PORT);
    
    if (mouse_state.packet_size == 4) {
        // 4-байтный пакет (с колесиком)
        switch (mouse_cycle) {
            case 0:
                // Первый байт - флаги
                if (data & 0x08) { // Всегда должен быть установлен
                    mouse_packet[0] = (int8_t)data;
                    mouse_cycle++;
                }
                break;
            case 1:
                // Второй байт - движение по X
                mouse_packet[1] = (int8_t)data;
                mouse_cycle++;
                break;
            case 2:
                // Третий байт - движение по Y
                mouse_packet[2] = (int8_t)data;
                mouse_cycle++;
                break;
            case 3:
                // Четвертый байт - колесико и дополнительные кнопки
                mouse_packet[3] = (int8_t)data;
                mouse_cycle = 0;
                
                // Обрабатываем данные
                // Обновляем кнопки
                mouse_state.buttons = mouse_packet[0] & 0x07;
                
                // Обрабатываем движение (если нужно)
                // mouse_state.x += (int32_t)mouse_packet[1];
                // mouse_state.y -= (int32_t)mouse_packet[2]; // Инвертируем Y
                
                // Обрабатываем колесико (байт 3)
                int8_t scroll = mouse_packet[3];
                
                // Проверяем, что это действительно данные колесика, а не кнопки
                if ((mouse_packet[3] & 0x0F) == 0) {
                    // Это колесико (значение в диапазоне -8..7)
                    mouse_state.scroll += (int32_t)scroll;
                }
                break;
        }
    } else {
        // 3-байтный пакет (без колесика) - игнорируем
        mouse_cycle = (mouse_cycle + 1) % 3;
    }
}

// Получить текущее состояние мыши
mouse_state_t get_mouse_state(void) {
    return mouse_state;
}