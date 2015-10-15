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

#include <Arduino.h>
#include "UbirchSIM800.h"

// debug AT i/o (very verbose)
//#define DEBUG_AT
// debug receiving and sending of packets (sizes)
//#define DEBUG_PACKETS
// debugging of send/receive progress (not very verbose)
#define DEBUG_PROGRESS

#ifndef NDEBUG
#   define PRINT(s) Serial.print(F(s))
#   define PRINTLN(s) Serial.println(F(s))
#   define DEBUG(...) Serial.print(__VA_ARGS__)
#   define DEBUGQ(...) Serial.print("'"); Serial.print(__VA_ARGS__); Serial.print("'")
#   define DEBUGLN(...) Serial.println(__VA_ARGS__)
#   define DEBUGQLN(...) Serial.print("'"); Serial.print(__VA_ARGS__); Serial.println("'")
#else
#   define PRINT(s)
#   define PRINTLN(s)
#   define DEBUG(...)
#   define DEBUGQ(...)
#   define DEBUGLN(...)
#   define DEBUGQLN(...)
#endif

#define println_param(prefix, p) print(F(prefix)); print(F(",\"")); print(p); println(F("\""));

UbirchSIM800::UbirchSIM800() {
}

bool UbirchSIM800::reset() {
    return reset(_serialSpeed);
}

bool UbirchSIM800::reset(uint32_t serialSpeed) {
    _serial.begin(serialSpeed);

    pinMode(SIM800_RST, OUTPUT);
    digitalWrite(SIM800_RST, HIGH);
    delay(10);
    digitalWrite(SIM800_RST, LOW);
    delay(100);
    digitalWrite(SIM800_RST, HIGH);

    delay(7000);

    while (_serial.available()) _serial.read();

    expect_AT_OK(F(""));
    expect_AT_OK(F(""));
    expect_AT_OK(F(""));
    bool ok = expect_AT_OK(F("E0"));

    expect_AT_OK(F("+IFC=0,0")); // No hardware flow control
    expect_AT_OK(F("+CIURC=0")); // No "Call Ready"

    while (_serial.available()) _serial.read();

    return ok;
}

void UbirchSIM800::setAPN(const __FlashStringHelper *apn, const __FlashStringHelper *user,
                          const __FlashStringHelper *pass) {
    _apn = apn;
    _user = user;
    _pass = pass;
}

bool UbirchSIM800::unlock(const __FlashStringHelper *pin) {
    print(F("+CPIN="));
    println(pin);
    return expect_OK();
}


bool UbirchSIM800::time(char *date, char *time, char *tz) {
    println(F("AT+CCLK?"));

    return expect_scan(F("+CCLK: \"%8s,%8s%3s\""), date, time, tz) == 3;
}

bool UbirchSIM800::IMEI(char *imei) {
    println(F("AT+GSN"));
    expect_scan(F("%s"), imei);
    return expect_OK();
}

bool UbirchSIM800::wakeup() {
    PRINTLN("!!! SIM800 wakeup");

    expect_AT_OK(F(""));
    // check if the chip is already awake, otherwise start wakeup
    if (!expect_AT_OK(F(""), 5000)) {
        PRINTLN("!!! using PWRKEY wakeup procedure");
        pinMode(SIM800_KEY, OUTPUT);
        do {
            digitalWrite(SIM800_KEY, HIGH);
            delay(10);
            digitalWrite(SIM800_KEY, LOW);
            delay(1100);
            digitalWrite(SIM800_KEY, HIGH);
            delay(2000);
        } while (digitalRead(SIM800_PS) == LOW);
        // make pin unused (do not leak)
        pinMode(SIM800_KEY, INPUT_PULLUP);
        PRINTLN("!!! SIM800 ok");
    } else {
        PRINTLN("!!! SIM800 already awake");
    }

    return reset();
}

bool UbirchSIM800::shutdown() {
    PRINTLN("!!! SIM800 shutdown");

    disableGPRS();
    if (!expect_AT(F("+CPOWD=1"), F("NORMAL POWER DOWN"), 5000)) {
        if (digitalRead(SIM800_PS) == HIGH) {
            PRINTLN("shutdown() using PWRKEY, AT+CPOWD=1 failed");
            pinMode(SIM800_KEY, OUTPUT);
            digitalWrite(SIM800_KEY, LOW);
            for (; digitalRead(SIM800_KEY) == LOW;);
            digitalWrite(SIM800_KEY, HIGH);
            pinMode(SIM800_KEY, INPUT_PULLUP);
        } else {
            PRINTLN("shutdown(): already shut down");
        }
    }
    PRINTLN("!!! SIM800 shutdown ok");
    return true;
}

bool UbirchSIM800::registerNetwork(uint16_t timeout) {
    PRINTLN("!!! waiting for network registration");
    expect_AT_OK(F(""));
    while (timeout -= 1000) {
        uint8_t n = 0;
        println(F("AT+CREG?"));
        expect_scan(F("+CREG: 0,%d"), &n);
#if !defined(NDEBUG) && defined(DEBUG_PROGRESS)
        switch (n) {
            case 0:
                PRINT("_");
                break;
            case 1:
                PRINT("H");
                break;
            case 2:
                PRINT("S");
                break;
            case 3:
                PRINT("D");
                break;
            case 4:
                PRINT("?");
                break;
            case 5:
                PRINT("R");
                break;
            default:
                DEBUG(n);
                break;
        }
#endif
        if ((n == 1 || n == 5)) {
            PRINTLN("");
            return true;
        }
        delay(1000);
    }
    return false;
}

bool UbirchSIM800::enableGPRS(uint16_t timeout) {
    expect_AT(F("+CIPSHUT"), F("SHUT OK"), 5000);
    expect_AT_OK(F("+CIPMUX=1")); // enable multiplex mode
    expect_AT_OK(F("+CIPRXGET=1")); // we will receive manually

    bool attached = false;
    while (!attached && timeout > 0) {
        attached = expect_AT_OK(F("+CGATT=1"), 10000);
        delay(1000);
	timeout -= 1000;
    }
    if (!attached) return false;

    if (!expect_AT_OK(F("+SAPBR=3,1,\"CONTYPE\",\"GPRS\""), 10000)) return false;

    // set bearer profile access point name
    if (_apn) {
        print(F("AT+SAPBR=3,1,\"APN\",\""));
        print(_apn);
        println(F("\""));
        if (!expect_OK()) return false;

        if (_user) {
            print(F("AT+SAPBR=3,1,\"USER\",\""));
            print(_user);
            println(F("\""));
            if (!expect_OK()) return false;
        }
        if (_pass) {
            print(F("AT+SAPBR=3,1,\"PWD\",\""));
            print(_pass);
            println(F("\""));
            if (!expect_OK()) return false;
        }
    }

    // open GPRS context
    expect_AT_OK(F("+SAPBR=1,1"), 30000);

    uint16_t gprsState;
    do {
        println(F("AT+CGATT?"));
        expect_scan(F("+CGATT: %d"), &gprsState);
        delay(1);
    } while (--timeout && !gprsState);

    return gprsState != 0;
}

bool UbirchSIM800::disableGPRS() {
    expect_AT(F("+CIPSHUT"), F("SHUT OK"));
    if (!expect_AT_OK(F("+SAPBR=0,1"))) return false;

    return expect_AT_OK(F("+CGATT=0"));
}

uint16_t UbirchSIM800::HTTP_get(const char *url, uint32_t &length) {
    expect_AT_OK(F("+HTTPTERM"));
    delay(100);

    if (!expect_AT_OK(F("+HTTPINIT"))) return 1000;
    if (!expect_AT_OK(F("+HTTPPARA=\"CID\",1"))) return 1101;
    if (!expect_AT_OK(F("+HTTPPARA=\"UA\",\"UBIRCH#1\""))) return 1102;
    if (!expect_AT_OK(F("+HTTPPARA=\"REDIR\",1"))) return 1103;
    println_param("AT+HTTPPARA=\"URL\"", url);
    if (!expect_OK()) return 1110;

    if (!expect_AT_OK(F("+HTTPACTION=0"))) return 1004;

    uint16_t status;
    expect_scan(F("+HTTPACTION: 0,%d,%lu"), &status, &length, 60000);

    return status;
}

uint16_t UbirchSIM800::HTTP_get(const char *url, uint32_t &length, STREAM &file) {
    uint16_t status = HTTP_get(url, length);
    PRINT("HTTP STATUS: ");
    DEBUGLN(status);
    PRINT("FILE LENGTH: ");
    DEBUGLN(length);

    if (length == 0) return status;

    char *buffer = (char *) malloc(SIM800_BUFSIZE);
    uint32_t pos = 0, r = 0;
    do {
        r = HTTP_get_read(buffer, pos, SIM800_BUFSIZE);
#if !defined(NDEBUG) && defined(DEBUG_PROGRESS)
        if ((pos % 10240) == 0) {
            PRINT(" ");
            DEBUGLN(pos);
        } else if (pos % (1024) == 0) { PRINT("<"); }
#endif
        pos += r;
        file.write(buffer, r);
    } while (pos < length);
    free(buffer);
    PRINTLN("");

    return status;
}

size_t UbirchSIM800::HTTP_get_read(char *buffer, uint32_t start, size_t length) {
    print(F("AT+HTTPREAD="));
    print(start);
    print(F(","));
    println(length);

    uint16_t available;
    expect_scan(F("+HTTPREAD: %lu"), &available);
#ifdef DEBUG_PACKETS
    PRINT("~~~ PACKET: ");
    DEBUGLN(available);
#endif
    size_t idx = read(buffer, available);
    if (!expect_OK()) return 0;
#ifdef DEBUG_PACKETS
    PRINT("~~~ DONE: ");
    DEBUGLN(idx);
#endif
    return idx;
}

uint16_t UbirchSIM800::HTTP_post(const char *url, uint32_t &length) {
    expect_AT_OK(F("+HTTPTERM"));
    delay(100);

    if (!expect_AT_OK(F("+HTTPINIT"))) return 1000;
    if (!expect_AT_OK(F("+HTTPPARA=\"CID\",1"))) return 1101;
    if (!expect_AT_OK(F("+HTTPPARA=\"UA\",\"UBIRCH#1\""))) return 1102;
    if (!expect_AT_OK(F("+HTTPPARA=\"REDIR\",1"))) return 1103;
    println_param("AT+HTTPPARA=\"URL\"", url);
    if (!expect_OK()) return 1110;

    if (!expect_AT_OK(F("+HTTPACTION=1"))) return 1001;

    uint16_t status;
    expect_scan(F("+HTTPACTION: 0,%d,%lu"), &status, &length, 60000);

    return status;
}

uint16_t UbirchSIM800::HTTP_post(const char *url, uint32_t &length, STREAM &file, uint32_t size) {
    expect_AT_OK(F("+HTTPTERM"));
    delay(100);

    if (!expect_AT_OK(F("+HTTPINIT"))) return 1000;
    if (!expect_AT_OK(F("+HTTPPARA=\"CID\",1"))) return 1101;
    if (!expect_AT_OK(F("+HTTPPARA=\"UA\",\"UBIRCH#1\""))) return 1102;
    if (!expect_AT_OK(F("+HTTPPARA=\"REDIR\",1"))) return 1103;
    println_param("AT+HTTPPARA=\"URL\"", url);
    if (!expect_OK()) return 1110;

    print(F("AT+HTTPDATA="));
    print(size);
    print(F(","));
    println((uint32_t) 120000);

    if (!expect(F("DOWNLOAD"))) return 0;

    uint8_t *buffer = (uint8_t *) malloc(SIM800_BUFSIZE);
    uint32_t pos = 0, r = 0;

    do {
        for (r = 0; r < SIM800_BUFSIZE; r++) {
            int c = file.read();
            if (c == -1) break;
            _serial.write((uint8_t) c);
        }

        if (r < SIM800_BUFSIZE) {
#if !defined(NDEBUG) && defined(DEBUG_PROGRESS)
            PRINTLN("EOF");
#endif
            break;
        }
#ifndef NDEBUG
        if ((pos % 10240) == 0) {
            PRINT(" ");
            DEBUGLN(pos);
        } else if (pos % (1024) == 0) { PRINT(">"); }
#endif
        pos += r;
    } while (r == SIM800_BUFSIZE);

    free(buffer);
    PRINTLN("");

    if (!expect_OK(5000)) return 1005;

    if (!expect_AT_OK(F("+HTTPACTION=1"))) return 1004;

    // wait for the action to be completed, give it 5s for each try
    uint16_t status;
    while (!expect_scan(F("+HTTPACTION: 1,%d,%lu"), &status, &length, 5000));

    return status;
}

inline size_t UbirchSIM800::read(char *buffer, size_t length) {
    uint32_t idx = 0;
    while (length) {
        while (length && _serial.available()) {
            buffer[idx++] = (char) _serial.read();
            length--;
        }
    }
    return idx;
}

bool UbirchSIM800::connect(const char *address, uint16_t port, uint16_t timeout) {
    if (!expect_AT(F("+CIPSHUT"), F("SHUT OK"))) return false;
    if (!expect_AT_OK(F("+CMEE=2"))) return false;
    if (!expect_AT_OK(F("+CIPQSEND=1"))) return false;

    // bring connection up, force it
    print(F("AT+CSTT=\""));
    print(_apn);
    println(F("\""));
    if (!expect_OK()) return false;

    if (!expect_AT_OK(F("+CIICR"))) return false;

    // try five times to get an IP address
    bool connected = false;
    do {
        char ipaddress[23];
        println(F("AT+CIFSR"));
        expect_scan(F("%s"), ipaddress);
        connected = strcmp_P(ipaddress, PSTR("ERROR")) != 0;
        if (!connected) delay(1);
    } while (timeout-- && !connected);

    if (!connected) return false;

    print(F("AT+CIPSTART=0,\"TCP\",\""));
    print(address);
    print(F("\",\""));
    print(port);
    println(F("\""));
    if (!expect_OK()) return false;
    if (!expect(F("0, CONNECT OK"), 30000)) return false;

    return connected;
}

bool UbirchSIM800::status() {
    println(F("AT+CIPSTATUS=0"));

    char status[SIM800_BUFSIZE];
    expect_scan(F("+CIPSTATUS: %s"), status);
    DEBUGLN(status);
    if (!expect_OK()) return false;

    return strcmp_P(status, PSTR("CONNECTED")) < 0;
}

bool UbirchSIM800::disconnect() {
    return expect_AT_OK(F("+CIPCLOSE=0"));
};

bool UbirchSIM800::send(char *buffer, size_t size, size_t &accepted) {
    print(F("AT+CIPSEND=0,"));
    println(size);

    if (!expect(F("> "))) return false;
    _serial.write(buffer, size);

    if (!expect_scan(F("DATA ACCEPT: 0,%lu"), &accepted, 3000)) {
        // we have a buffer of 319488 bytes, so we are optimistic,
        // even if a temporary fail occurs and just carry on
        // (verified!)
        //return false;
    }

    return accepted == size;
}

size_t UbirchSIM800::receive(char *buffer, size_t size) {
    size_t actual = 0;
    while (actual < size) {
        uint8_t chunk = min(size - actual, 128);
        print(F("AT+CIPRXGET=2,0,"));
        println(chunk);

        uint32_t requested, confirmed;
        if (!expect_scan(F("+CIPRXGET: 2,%*d,%lu,%u"), &requested, &confirmed)) return 0;

        actual += read(buffer, confirmed);
    }

    return actual;
}

/* ===========================================================================
 * PROTECTED
 * ===========================================================================
 */

// read a line
unsigned int UbirchSIM800::readline(char *buffer, size_t max, uint16_t timeout) {
    uint16_t idx = 0;
    while (--timeout) {
        while (_serial.available()) {
            char c = (char) _serial.read();
            if (c == '\r') continue;
            if (c == '\n') {
                if (!idx) continue;
                timeout = 0;
                break;
            }
            if (max - idx) buffer[idx++] = c;
        }

        if (timeout == 0) break;
        delay(1);
    }
    buffer[idx] = 0;
    return idx;
};

void UbirchSIM800::eatEcho() {
    while (_serial.available()) {
        _serial.read();
        // don't be too quick or we might not have anything available
        // when there actually is...
        delay(1);
    }
}

void UbirchSIM800::print(const __FlashStringHelper *s) {
#ifdef DEBUG_AT
    PRINT("+++ ");
    DEBUGQLN(s);
#endif
    _serial.print(s);
}

void UbirchSIM800::print(uint32_t s) {
#ifdef DEBUG_AT
    PRINT("+++ ");
    DEBUGLN(s);
#endif
    _serial.print(s);
}


void UbirchSIM800::print(const char *s) {
#ifdef DEBUG_AT
    PRINT("+++ ");
    DEBUGQLN(s);
#endif
    _serial.print(s);
}

void UbirchSIM800::println(const __FlashStringHelper *s) {
#ifdef DEBUG_AT
    PRINT("+++ ");
    DEBUGQLN(s);
#endif
    _serial.print(s);
    eatEcho();
    _serial.println();
}

void UbirchSIM800::println(uint32_t s) {
#ifdef DEBUG_AT
    PRINT("+++ ");
    DEBUGLN(s);
#endif
    _serial.print(s);
    eatEcho();
    _serial.println();
}

void UbirchSIM800::println(const char *s) {
#ifdef DEBUG_AT
    PRINT("+++ ");
    DEBUGQLN(s);
#endif
    _serial.print(s);
    eatEcho();
    _serial.println();
}

bool UbirchSIM800::expect_AT(const __FlashStringHelper *cmd, const __FlashStringHelper *expected, uint16_t timeout) {
    print(F("AT"));
    println(cmd);
    return expect(expected, timeout);
}

bool UbirchSIM800::expect_AT_OK(const __FlashStringHelper *cmd, uint16_t timeout) {
    return expect_AT(cmd, F("OK"), timeout);
}

bool UbirchSIM800::expect(const __FlashStringHelper *expected, uint16_t timeout) {
    char buf[SIM800_BUFSIZE];
    unsigned int len = readline(buf, SIM800_BUFSIZE, timeout);
#ifdef DEBUG_AT
    PRINT("--- (");
    DEBUG(len);
    PRINT(") ");
    DEBUGQLN(buf);
#endif
    return strcmp_P(buf, (char PROGMEM *) expected) == 0;
}

bool UbirchSIM800::expect_OK(uint16_t timeout) {
    return expect(F("OK"), timeout);
}

bool UbirchSIM800::expect_scan(const __FlashStringHelper *pattern, void *ref, uint16_t timeout) {
    char buf[SIM800_BUFSIZE];
    unsigned int len = readline(buf, SIM800_BUFSIZE, timeout);
#ifdef DEBUG_AT
    PRINT("--- (");
    DEBUG(len);
    PRINT(") ");
    DEBUGQLN(buf);
#endif
    return sscanf_P(buf, (const char PROGMEM *) pattern, ref) == 1;
}

bool UbirchSIM800::expect_scan(const __FlashStringHelper *pattern, void *ref, void *ref1, uint16_t timeout) {
    char buf[SIM800_BUFSIZE];
    unsigned int len = readline(buf, SIM800_BUFSIZE, timeout);
#ifdef DEBUG_AT
    PRINT("--- (");
    DEBUG(len);
    PRINT(") ");
    DEBUGQLN(buf);
#endif
    return sscanf_P(buf, (const char PROGMEM *) pattern, ref, ref1) == 2;
}

bool UbirchSIM800::expect_scan(const __FlashStringHelper *pattern, void *ref, void *ref1, void *ref2,
                               uint16_t timeout) {
    char buf[SIM800_BUFSIZE];
    unsigned int len = readline(buf, SIM800_BUFSIZE, timeout);
#ifdef DEBUG_AT
    PRINT("--- (");
    DEBUG(len);
    PRINT(") ");
    DEBUGQLN(buf);
#endif
    return sscanf_P(buf, (const char PROGMEM *) pattern, ref, ref1, ref2) == 1;
}

