#include <linux/uinput.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include "rpi_gpio.h"

#define UINPUT_DEV_NAME "737 FMC"
const int cols[] = { 14, 15, 18, 23, 24, 25, 8, 7, 16 };
const int ncols = 9;
const int rows[] = { 21, 2, 3, 4, 17, 27, 22, 10 };
const int nrows = 8;
const int keymap[] = {
    KEY_F1,  KEY_F9,        KEY_RIGHTBRACE,  KEY_LEFTCTRL, KEY_F,     KEY_N,  KEY_V,     KEY_ESC,   KEY_5,
    KEY_F2,  KEY_F10,       KEY_SEMICOLON,   KEY_PAGEUP,   KEY_G,     KEY_O,  KEY_W,     KEY_DOT,   KEY_6,
    KEY_F3,  KEY_F11,       KEY_APOSTROPHE,  KEY_PAGEDOWN, KEY_H,     KEY_P,  KEY_X,     KEY_0,     KEY_1,
    KEY_F4,  KEY_F12,       KEY_BACKSLASH,   KEY_A,        KEY_I,     KEY_Q,  KEY_Y,     KEY_MINUS, KEY_2,
    KEY_F5,  KEY_INSERT,    KEY_GRAVE,       KEY_B,        KEY_J,     KEY_R,  KEY_Z,     KEY_7,     KEY_3,
    KEY_F6,  KEY_HOME,      KEY_EQUAL,       KEY_C,        KEY_K,     KEY_S,  KEY_MINUS, KEY_8,     KEY_LEFTALT,
    KEY_F7,  KEY_END,       KEY_ENTER,       KEY_D,        KEY_L,     KEY_T,  KEY_DELETE,KEY_9,     KEY_LEFTALT,
    KEY_F8,  KEY_LEFTBRACE, KEY_KPASTERISK,  KEY_E,        KEY_M,     KEY_U,  KEY_SLASH, KEY_4,     KEY_LEFTALT,
};

int uinput_init() {
    struct uinput_setup usetup;
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) return -1;
    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    ioctl(fd, UI_SET_EVBIT, EV_REP);
    for (int i = 0; i < ncols * nrows; i++) {
        ioctl(fd, UI_SET_KEYBIT, keymap[i]);
    }
    memset(&usetup, 0, sizeof(usetup));
    usetup.id.bustype = BUS_USB;
    usetup.id.vendor = 0x1209; // Generic
    usetup.id.product = 0x7501;
    strcpy(usetup.name, UINPUT_DEV_NAME);
    ioctl(fd, UI_DEV_SETUP, &usetup);
    ioctl(fd, UI_DEV_CREATE);
    return fd;
}
void uinput_emit(int fd, int type, int code, int val) {
   struct input_event ie;
   ie.type = type;
   ie.code = code;
   ie.value = val;
   ie.time.tv_sec = 0;
   ie.time.tv_usec = 0;
   write(fd, &ie, sizeof(ie));
}

int main() {
    int uinput_fd = uinput_init();
    if (uinput_fd < 0) {
        perror("Failed to initialize UInput");
        return -1;
    }
    if (rpi_gpio_init() < 0) {
        perror("Failed to initialize RPi GPIO");
        return -1;
    }
    for (int i = 0; i < nrows; i++) {
        rpi_gpio_setup(rows[i], INPUT, PUD_OFF);
    }
    for (int i = 0; i < ncols; i++) {
        rpi_gpio_setup(cols[i], INPUT, PUD_UP);
    }
    int pressed[nrows * ncols];
    memset(pressed, 0, sizeof(pressed));
    for (;;) {
        usleep(1000000 / 60);
        int syn = 0;
        for (int i = 0; i < nrows; i++) {
            rpi_gpio_output(rows[i], 0);
            rpi_gpio_set_fn(rows[i], OUTPUT);
            usleep(3);
            uint32_t all = rpi_gpio_input_all();
            rpi_gpio_set_fn(rows[i], INPUT);
            for (int j = 0; j < ncols; j++) {
                int keycode = i * ncols + j;
                int val = !(all & (1 << cols[j]));
                if (val != pressed[keycode]) {
                    pressed[keycode] = val;
                    uinput_emit(uinput_fd, EV_KEY, keymap[keycode], val);
                    syn++;
                }
            }
        }
        if (syn) uinput_emit(uinput_fd, EV_SYN, SYN_REPORT, 0);
    }
    return 0;
}
