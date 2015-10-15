/** 
 * A simple modem terminal.
 * 
 * This sketch simply proxies the serial port input and output
 * to the SIM800H chip on board of the ubirch #1. Makes it easy
 * to explore the AT command set.
 * 
 * Use a real serial terminal to access the ubirch #1:
 * 
 * $ screen /dev/cu.SLAB_USBtoUART 115200
 * 
 * Exchange the device with the one the Arduino IDE uses.
 * 
 * @author Matthias L. Jugel
 * 
 * == LICENSE ==
 * Copyright 2015 ubirch GmbH
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
  */

#include <Arduino.h>
#include <SoftwareSerial.h>
#include <UbirchSIM800.h>
#include "config.h" // copy from template and

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
#ifndef BAUD
#   define BAUD 115200
#endif

#define WATCHDOG 6

static const char *const url = "http://ubirch.com/";
UbirchSIM800 sim800 = UbirchSIM800();

void setup() {
    // disable the external watchdog
    pinMode(WATCHDOG, OUTPUT);

    // setup baud rates for serial and modem
    Serial.begin(BAUD);

    delay(3000);

    Serial.println("SIM800 wakeup...");
    if (!sim800.wakeup()) {
        Serial.println("SIM800 wakeup error");
        while (1);
    }
    sim800.setAPN(F(SIM800_APN), F(SIM800_USER), F(SIM800_PASS));

    Serial.println("SIM800 waiting for network registration...");
    while (!sim800.registerNetwork()) {
        sim800.shutdown();
        sim800.wakeup();
    }

    Serial.println("SIM800 enabling GPRS...");
    if (!sim800.enableGPRS()) {
        Serial.println("SIM800 can't enable GPRS");
        while (1);
    }
    Serial.println("SIM800 initialized");


    Serial.print("GET ");
    Serial.println(url);

    // for some reason this has to be static or it will not return correctly (seems like the stack
    // breaks because of the interrupt driven software serial
    static uint32_t length = 0;
    uint16_t status = sim800.HTTP_get(url, length);
    Serial.print("STATUS: "); Serial.println(status);
    Serial.print("RESPONSE LENGTH: "); Serial.println(length);

    sim800.expect_AT_OK(F("E1"));

    Serial.println("Entering Console Mode:");

    cli();

    // reset the timer registers
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1 = 0;

    OCR1A = (uint16_t) (16000000UL / 8 - 1); // every 128 microseconds
    TCCR1B |= _BV(CS01); // prescale 8 selected (still fast enough)
    TCCR1B |= _BV(WGM12); // CTC mode
    TIMSK1 |= _BV(OCIE1A); // timer compare interrupt

    sei();

}

// read what is available from the serial port and send to modem
ISR(TIMER1_COMPA_vect) {
    while (Serial.available() > 0) sim800._serial.write((uint8_t) Serial.read());
}

// the main loop just reads the responses from the modem and
// writes them to the serial port
void loop() {
    while (sim800._serial.available() > 0) Serial.write(sim800._serial.read());
}

#pragma clang diagnostic pop