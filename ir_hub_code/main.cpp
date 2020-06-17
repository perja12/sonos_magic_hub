#include <Arduino.h>
#include <avr/interrupt.h>
#include <avr/power.h>
#include <avr/sleep.h>
#include <util/delay.h>

#include "tiny_IRremote.h"

#define IR_RECV_PIN 2
#define INDICATOR_PIN 3

const uint32_t NEC_VOLUME_UP   = 0x20DF40BF;
const uint32_t NEC_VOLUME_DOWN = 0x20DFC03F;
const uint32_t NEC_VOLUME_MUTE = 0x20DF906F;
const uint32_t NEC_REPEAT      = 0xFFFFFFFF;

const uint16_t RC6_VOLUME_UP   = 0x1010;
const uint16_t RC6_VOLUME_DOWN = 0x1011;
const uint16_t RC6_VOLUME_MUTE = 0x100D;

// Limit the number of repeats to avoid damaging speaker element.
const uint8_t MAX_REPEATS = 5;

IRsend irsend;
IRrecv irrecv(IR_RECV_PIN);

uint32_t last_code = 0;
uint8_t num_repeats = 0;

EMPTY_INTERRUPT(PCINT0_vect);

void blink(uint8_t num) {
  for (uint8_t i = 0; i < num; i++) {
    PORTB |= (1 << INDICATOR_PIN);
    _delay_ms(20);
    PORTB &= ~(1 << INDICATOR_PIN);
    _delay_ms(20);
  }
}

void setup() {
  set_sleep_mode(SLEEP_MODE_IDLE);
  irrecv.enableIRIn();

  // Turn off ADC and USI to save power.
  ADCSRA &= ~ bit(ADEN);
  power_adc_disable();
  power_usi_disable();

  // Configure DDB0 and DDB1 as output to save power (unused).
  // In addition PB4 is configured by tiny_IRremote.
  DDRB = (1 << INDICATOR_PIN) | (1 << DDB0) | (1 << DDB1);

  PCMSK  |= _BV (PCINT2);  // enable pin change interrupt for PCINT2 pin (IR_RECV_PIN)
  GIFR   |= bit (PCIF);    // clear any outstanding interrupts
  GIMSK  |= bit (PCIE);    // enable pin change interrupts

  blink(3);
}

void send(uint32_t code) {
  irsend.sendRC6(code, 20);
  last_code = code;
  num_repeats = 0;
}

void send_last_code() {
  if (num_repeats < MAX_REPEATS) {
    irsend.sendRC6(last_code, 20);
    num_repeats++;
    _delay_ms(50);
  }
}

void loop() {
  decode_results results;

  // The IR library uses timer1 to collect raw IR data.
  // This is done in chunks of 50 ms. Repeatedly check for
  // data until there is data to decode or there is a timeout.
  uint32_t decode_start = millis();
  while (!irrecv.decode(&results)) {
    if ((millis() - decode_start) > 100) {
      break;
    }
  }

  if (irrecv.decode(&results)) {
    if (results.decode_type == NEC) {
      blink(1);
      switch(results.value) {
        case NEC_VOLUME_UP: send(RC6_VOLUME_UP); break;
        case NEC_VOLUME_DOWN: send(RC6_VOLUME_DOWN); break;
        case NEC_VOLUME_MUTE: send(RC6_VOLUME_MUTE); break;
        case NEC_REPEAT: send_last_code(); break;
      }
    }
    irrecv.enableIRIn();
    irrecv.resume();
  }

  power_timer0_disable();
  power_timer1_disable();
  sleep_mode();
  power_timer0_enable();
  power_timer1_enable();
}
