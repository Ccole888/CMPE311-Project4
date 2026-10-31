#include "Timer.h"
#include "pwm.h"

// Motor driver PWM: D11. Normally-open button: D7 to GND.

#define LED_1_STATE (1 << 0)
#define LED_2_STATE (1 << 1)
#define WAITING_PIN (1 << 2)
#define WAITING_INT (1 << 3)
#define INPUT_PIN (1 << 4)

uint8_t flags = 0;
char inputBuffer;
unsigned long currentTime = 0;

struct LED {
  uint16_t blinkIntervalMS = 0;
  unsigned long blinkStartTime = 0;
};

LED led1;
LED led2;

void taskLED(LED& led, uint8_t pin, uint8_t stateMask) {
  if (currentTime - led.blinkStartTime > led.blinkIntervalMS / 2) {
    flags ^= stateMask;
    digitalWrite(pin, (flags & stateMask) ? HIGH : LOW);
    led.blinkStartTime = currentTime;
  }
}

void taskSerial() {
  if (flags & WAITING_PIN) {
    if (Serial.available() > 0) {
      inputBuffer = Serial.read();

      switch (inputBuffer) {
        case '\r':
        case '\n':
          break;

        case '1':
        case '2':
          if (inputBuffer == '2') {
            flags |= INPUT_PIN;
          } else {
            flags &= ~INPUT_PIN;
          }

          flags &= ~WAITING_PIN;
          flags |= WAITING_INT;
          Serial.println("Enter an interval from 1 to 60000 ms:");
          break;

        default:
          if ((unsigned char)inputBuffer >= 32 &&
              (unsigned char)inputBuffer <= 126) {
            Serial.println("Invalid LED. Enter 1 or 2:");
          }
          break;
      }
    }
  } else if (flags & WAITING_INT) {
    static unsigned long value = 0;
    static bool hasDigits = false;
    static bool invalid = false;

    while (Serial.available() > 0) {
      char c = Serial.read();

      if (c == '\r') continue;

      if (c == '\n') {
        if (!hasDigits && !invalid) continue;

        bool valid = hasDigits && !invalid && value > 0;

        if (valid) {
          if (flags & INPUT_PIN) {
            led2.blinkIntervalMS = value;
            flags &= ~WAITING_INT;
          } else {
            led1.blinkIntervalMS = value;
            flags &= ~WAITING_INT;
          }
        } else {
          Serial.println("Enter an interval from 1 to 60000 ms:");
        }

        value = 0;
        hasDigits = false;
        invalid = false;

        if (valid) break;
      } else if (c >= '0' && c <= '9') {
        hasDigits = true;

        if (!invalid) {
          value = value * 10 + (c - '0');
          if (value > 60000) invalid = true;
        }
      } else {
        invalid = true;
      }
    }
  } else {
    Serial.println(F("What LED? (1 or 2)"));
    flags |= WAITING_PIN;
  }
}

void taskLED1() {
  taskLED(led1, 2, LED_1_STATE);
}

void taskLED2() {
  taskLED(led2, 3, LED_2_STATE);
}

// Use the same millisecond sample as the LED tasks.
void taskMotor() {
  updateMotor(currentTime);
}

void (*tasks[])() = {
  taskMotor,
  taskLED1,
  taskLED2,
  taskSerial
};

const size_t taskCount = sizeof(tasks) / sizeof(tasks[0]);

void setup() {
  Serial.begin(115200);
  delay(1000);
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);

  led1.blinkIntervalMS = 1000;
  led2.blinkIntervalMS = 1000;

  digitalWrite(2, HIGH);
  digitalWrite(3, HIGH);

  flags |= LED_1_STATE | LED_2_STATE;
  myMilliBegin();
  currentTime = myMilli();
  led1.blinkStartTime = currentTime;
  led2.blinkStartTime = currentTime;
}

void loop() {
  currentTime = myMilli();
  for (size_t i = 0; i < taskCount; i++) {
    tasks[i]();
  }
}
