#ifndef DIGITAL_SPEEDOMETER_H
#define DIGITAL_SPEEDOMETER_H

#include <Arduino.h>

#define STATE_DISCONNECTED 0x0
#define STATE_CONNECTED    0x2
#define STATE_SLEEPING     0x3

#define TIMER_INTERVAL_DISP_REFRESH_MS 10
#define TIMER_INTERVAL_DISP_INC_MS   500

#define PIN_SEG_A  5
#define PIN_SEG_B  6
#define PIN_SEG_C  7
#define PIN_SEG_D  8
#define PIN_SEG_E  9
#define PIN_SEG_F  10
#define PIN_SEG_G  11
#define PIN_SEG_DP 12

#define PIN_DIG_1 A1
#define PIN_DIG_2 A2

#define PIN_USE_IMPERIAL 2
#define PIN_USE_METRIC   3

#define PIN_SPEED_ADJUST 0

//#define MODE_SIMULATION

typedef uint8_t speed_t;
typedef uint8_t state_t;

void setup_timers();
void setup_interrupts();
void setup_display();
void setup_obd_connection();

void isr_display();
void isr_refresh_display();
void isr_read_display_unit();

speed_t adjust_speed(speed_t);
void set_displayed_speed(speed_t);
void probe_current_speed();

int analog_read_avg(const int, const int, const long);

void enter_sleep_mode();
void leave_sleep_mode();

#endif // !DIGITAL_SPEEDOMETER_H
