#pragma once

#ifndef MOTOR_DEBUG
#define MOTOR_DEBUG 1
#endif
#if MOTOR_DEBUG
#include <Arduino.h> // Temporary serial debugging only.
#endif

#if !defined(__AVR_ATmega328P__)
#error "Motor PWM requires an ATmega328P (Uno R3 or classic Nano)."
#endif
#if !defined(F_CPU) || F_CPU != 16000000UL
#error "Motor PWM requires a 16 MHz CPU clock."
#endif

// Pin/PWM access uses direct ATmega328P data-space addresses.
// Debug output uses Serial; initialize it in your existing setup().
// Set MOTOR_DEBUG to 0 before including this header to remove Serial entirely.
// D7/PD7: normally-open button to GND; internal pull-up enabled here.
// D11/PB3/OC2A: active-high motor-driver input, NOT motor power.
// Reserves Timer2 for PWM; do not use tone() or other Timer2 clients.
// Timer1 remains dedicated to Timer.h/myMilli(). Timer0 is also untouched.
// Timer2 channel B stays disconnected, so D3 remains a normal LED output.
// Call only from the main loop, frequently, passing an atomic 32-bit millisecond
// sample from your existing time source. No separate initialization is needed.
static inline void updateMotor(unsigned long nowMs)
{
    volatile unsigned char * const ddrB   = (volatile unsigned char *)0x24;
    volatile unsigned char * const portB  = (volatile unsigned char *)0x25;
    volatile unsigned char * const pinD   = (volatile unsigned char *)0x29;
    volatile unsigned char * const ddrD   = (volatile unsigned char *)0x2A;
    volatile unsigned char * const portD  = (volatile unsigned char *)0x2B;
    volatile unsigned char * const timsk2 = (volatile unsigned char *)0x70;
    volatile unsigned char * const tccr2a = (volatile unsigned char *)0xB0;
    volatile unsigned char * const tccr2b = (volatile unsigned char *)0xB1;
    volatile unsigned char * const tcnt2  = (volatile unsigned char *)0xB2;
    volatile unsigned char * const ocr2a  = (volatile unsigned char *)0xB3;

    // OFF -> LOW -> MEDIUM -> HIGH -> MEDIUM -> LOW -> repeat.
    // Starting values only; calibrate airflow, noise, and reliable startup.
    static const unsigned char duty[] = {0, 100, 180, 255, 180, 100};
    static unsigned char initialized = 0;
    static unsigned char step = 0;
    static unsigned char lastRaw = 1;
    static unsigned char stable = 1;
    static unsigned long changedAt = 0;

    if (!initialized) {
        *tccr2b = 0;                     // Stop Timer2 while configuring.
        *timsk2 = 0;                     // Hardware PWM needs no interrupts.
        *tccr2a = 0;                     // Disconnect both compare outputs.
        *portB &= (unsigned char)~0x08u; // D11 output latch LOW.
        *ddrB |= 0x08u;                 // D11 output; preserve other pins.
        *ddrD &= (unsigned char)~0x80u;  // D7 input.
        *portD |= 0x80u;                // D7 internal pull-up.
        *tcnt2 = 0;
        *ocr2a = 0;
        *tccr2a = 0x01u;                // Phase-correct PWM; D11 off.
        *tccr2b = 0x01u;                // Clock / 1: 16 MHz / 510 = 31.37 kHz.

        lastRaw = stable = (*pinD & 0x80u) ? 1u : 0u;
        changedAt = nowMs;
        initialized = 1;
        return; // If held at boot, release before the first press counts.
    }

    const unsigned char raw = (*pinD & 0x80u) ? 1u : 0u;
    if (raw != lastRaw) {
        lastRaw = raw;
        changedAt = nowMs;
    }

    // Nonblocking debounce; unsigned subtraction handles 32-bit rollover.
    if (raw != stable && (unsigned long)(nowMs - changedAt) >= 25UL) {
        stable = raw;
        if (stable == 0) {
            if (++step == sizeof(duty) / sizeof(duty[0])) {
                step = 0;
            }
            *ocr2a = duty[step];
            // COM2A1 enables non-inverting OC2A PWM. Disconnect for exact OFF;
            // the D11 port latch remains LOW while PWM is disconnected.
            *tccr2a = duty[step] ? 0x81u : 0x01u;

#if MOTOR_DEBUG
            Serial.print(F("Button press detected -> "));
            switch (step) {
                case 0: Serial.println(F("OFF"));    break;
                case 1:
                case 5: Serial.println(F("LOW"));    break;
                case 2:
                case 4: Serial.println(F("MEDIUM")); break;
                case 3: Serial.println(F("HIGH"));   break;
            }
#endif
        }
    }
}
