#ifndef MY_MILLI_H
#define MY_MILLI_H

#include <stdint.h>

// Custom defined data memory adresses
#define MY_TIMER1_CONTROL_A      (*(volatile uint8_t *)0x80)
#define MY_TIMER1_CONTROL_B      (*(volatile uint8_t *)0x81)
#define MY_TIMER1_COUNT_HIGH     (*(volatile uint8_t *)0x85)
#define MY_TIMER1_COUNT_LOW      (*(volatile uint8_t *)0x84)
#define MY_TIMER1_COMPARE_HIGH   (*(volatile uint8_t *)0x89)
#define MY_TIMER1_COMPARE_LOW    (*(volatile uint8_t *)0x88)
#define MY_TIMER1_INTERRUPT_MASK (*(volatile uint8_t *)0x6F)
#define MY_TIMER1_FLAGS          (*(volatile uint8_t *)0x36)
#define MY_CPU_STATUS            (*(volatile uint8_t *)0x5F)

// bit masks and timer constants
#define MY_TIMER1_CTC_MODE            (1u << 3)
#define MY_TIMER1_DIVIDE_BY_64        ((1u << 1) | (1u << 0))
#define MY_TIMER1_COMPARE_A_INTERRUPT (1u << 1)
#define MY_TIMER1_COMPARE_A_FLAG      (1u << 1)
#define MY_TIMER1_COMPARE_VALUE       249u

//  AVR startup vector table 
#define MY_TIMER1_COMPARE_HANDLER __vector_11

static volatile uint32_t myMilliCount = 0;

// replace the supplied ISR() helper macro.
extern "C" void MY_TIMER1_COMPARE_HANDLER(void)
    __attribute__((signal, used, externally_visible));

extern "C" void MY_TIMER1_COMPARE_HANDLER(void) {
    ++myMilliCount;
}

static inline void myMilliBegin(void) {
    const uint8_t savedStatus = MY_CPU_STATUS;
    // A CPU instruction, not the AVR cli() helper macro.
    __asm__ __volatile__("cli" ::: "memory");

    MY_TIMER1_INTERRUPT_MASK = 0;
    MY_TIMER1_CONTROL_B = 0;  // Stop the timer before changing it
    MY_TIMER1_CONTROL_A = 0;  // Clear remaining mode/output settings

    // Timer1's 16-bit registers require high-byte-first writes
    MY_TIMER1_COUNT_HIGH = 0;
    MY_TIMER1_COUNT_LOW = 0;
    MY_TIMER1_COMPARE_HIGH = 0;
    MY_TIMER1_COMPARE_LOW = MY_TIMER1_COMPARE_VALUE;

    myMilliCount = 0;
    // This flag is cleared by writing a ONE, not a zero
    MY_TIMER1_FLAGS = MY_TIMER1_COMPARE_A_FLAG;
    MY_TIMER1_INTERRUPT_MASK = MY_TIMER1_COMPARE_A_INTERRUPT;

    // 16,000,000 / 64 / (249 + 1) = 1,000 interrupts per second
    MY_TIMER1_CONTROL_B =
        MY_TIMER1_CTC_MODE | MY_TIMER1_DIVIDE_BY_64;

    // Preserve the caller's global interrupt state
    MY_CPU_STATUS = savedStatus;
    __asm__ __volatile__("" ::: "memory");
}

static inline uint32_t myMilli(void) {
    const uint8_t savedStatus = MY_CPU_STATUS;
    __asm__ __volatile__("cli" ::: "memory");

    // Prevent an ISR from changing it partway through this read.
    const uint32_t snapshot = myMilliCount;

    MY_CPU_STATUS = savedStatus;
    __asm__ __volatile__("" ::: "memory");
    return snapshot;
}


#endif