# ubirch-avr-sim800

C++ library to drive the SIM800 chip.

I wrote this code to make sure we can send and retrieve data using the chip
with minimal RAM and Flash impact. Additionally this class contains code to
safely shutdown and wake up the SIM800.

The example contains the necessary initialization code as well as how to
retrieve a URL using HTTP GET. It does not, however, retrieve the body of
the response. To do so is very simple and uses the Arduino Stream class.

Apart from the GET request, you can play with the AT commands of the SIM800
by connecting a serial console to your board.

## Works with ...

- Arduino compatible boards (AVR, ARM)
- Teensy-LC, Teensy 3.1/3.2

## LICENSE

    Copyright 2015 ubirch GmbH (http://www.ubirch.com)
    
    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at
    
        http://www.apache.org/licenses/LICENSE-2.0
    
    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
