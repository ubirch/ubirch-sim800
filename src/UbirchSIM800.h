/**
 * UbirchSIM800 is a class interfaces with the SIM800 chip to
 * provide functionality for uploading and downloading data via
 * the mobile phone network.
 *
 * @author Matthias L. Jugel
 *
 * Copyright 2015 ubirch GmbH (http://www.ubirch.com)
 *
 * == LICENSE ==
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef UBIRCH_SIM800_H
#define UBIRCH_SIM800_H

// SIM800H settings
#include <stdint.h>
#include <SoftwareSerial.h>
#include <Stream.h>

#define STREAM Stream

#define SIM800_BAUD 57600
#define SIM800_RX   2
#define SIM800_TX   3
#define SIM800_RST  4
#define SIM800_KEY  7
#define SIM800_PS   8

#define SIM800_CMD_TIMEOUT 30000
#define SIM800_SERIAL_TIMEOUT 1000
#define SIM800_BUFSIZE 64

class UbirchSIM800 {

public:
    UbirchSIM800();

    // stores apn, username and password for the time beeing
    void setAPN(const __FlashStringHelper *apn, const __FlashStringHelper *user, const __FlashStringHelper *pass);

    bool unlock(const __FlashStringHelper *pin);

    // reset the SIM800
    bool reset();

    // resets the SIM chip (if the chip is already awake)
    bool reset(uint32_t serialSpeed);

    // shut down the SIM chip to reduce power usage
    bool shutdown();

    // wake up the chip, checks if it's already awake, resets the chip (see #reset())
    bool wakeup();

    // wait for network registration
    bool registerNetwork(uint16_t timeout = SIM800_CMD_TIMEOUT);

    // enable GPRS
    bool enableGPRS(uint16_t timeout = SIM800_CMD_TIMEOUT);

    // disable GPRS
    bool disableGPRS();

    // get time off the SIM800 RTC
    bool time(char *date, char *time, char *tz);

    bool IMEI(char *imei);

    // query status of the network connection
    bool status();

    // connect a pure network connection, may send() data after it is opened
    bool connect(const char *address, uint16_t port, uint16_t timeout = SIM800_CMD_TIMEOUT);

    // disconnect a pure network connection
    bool disconnect();

    // send data down a pure network connection
    bool send(char *buffer, size_t size, size_t &accepted);

    size_t receive(char *buffer, size_t size);

    /**
     * HTTP requests only handle data up to 319488 bytes
     * This seems to be a limitation of the chip, a
     * larger payload will result in a 602 No Memory
     * result code
     */

    // HTTP GET request, returns the status and puts length in the referenced variable
    uint16_t HTTP_get(const char *url, uint32_t &length);

    // HTTP GET request, stores the received data in the stream (if length is > 0)
    uint16_t HTTP_get(const char *url, uint32_t &length, STREAM &file);

    // manually read the payload after a GET request, returns the amount read, call multiple times to read whole
    size_t HTTP_get_read(char *buffer, uint32_t start, size_t length);

    // HTTP HTTP_post request, returns the status
    uint16_t HTTP_post(const char *url, uint32_t &length);

    // HTTP HTTP_post request, reads the data from the stream and returns the result
    uint16_t HTTP_post(const char *url, uint32_t &length, STREAM &file, uint32_t size);

    // send a command (without AT) and expect it to return a certain string
    bool expect_AT(const __FlashStringHelper *cmd, const __FlashStringHelper *expected,
                   uint16_t timeout = SIM800_SERIAL_TIMEOUT);

    // send command (without AT) and expect OK
    bool expect_AT_OK(const __FlashStringHelper *cmd, uint16_t timeout = SIM800_SERIAL_TIMEOUT);

    // expect the string to be sent
    bool expect(const __FlashStringHelper *expected, uint16_t timeout = SIM800_SERIAL_TIMEOUT);

    // expect OK
    bool expect_OK(uint16_t timeout = SIM800_SERIAL_TIMEOUT);


    // expect a certain pattern with one value to be returned, ref is a pointer to the value
    bool expect_scan(const __FlashStringHelper *pattern, void *ref,
                     uint16_t timeout = SIM800_SERIAL_TIMEOUT);

    // expect a certain pattern with two values to be returned, ref, ref1 are pointer to the values
    bool expect_scan(const __FlashStringHelper *pattern, void *ref, void *ref1,
                     uint16_t timeout = SIM800_SERIAL_TIMEOUT);

    bool expect_scan(const __FlashStringHelper *pattern, void *ref, void *ref1, void *ref2,
                     uint16_t timeout = SIM800_SERIAL_TIMEOUT);

    SoftwareSerial _serial = SoftwareSerial(SIM800_TX, SIM800_RX);

protected:
    const uint32_t _serialSpeed = SIM800_BAUD;
    const __FlashStringHelper *_apn;
    const __FlashStringHelper *_user;
    const __FlashStringHelper *_pass;

    // read raw data
    size_t read(char *buffer, size_t length);

    // read a single line into the given buffer
    size_t readline(char *buffer, size_t max, uint16_t timeout);

    // eat input until no more is available, basically sucks up echos and left over status messages
    void eatEcho();

    void print(const __FlashStringHelper *s);

    void print(const char *s);

    void print(uint32_t s);

    void println(const __FlashStringHelper *s);

    void println(const char *s);

    void println(uint32_t s);

};

#endif //UBIRCH_SIM800_H
