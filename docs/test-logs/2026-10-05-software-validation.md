# Software validation record: 5 October 2026

Reviewed source revision: `d7dccd5`. Documentation changes followed this validation; firmware behavior was not changed. The board was not flashed or physically exercised by the reviewer.

## Host feedback tests

Command executed:
```sh
g++ -std=c++11 -Wall -Wextra -pedantic tests/proximity_feedback_test.cpp -o /tmp/intelligent-cane-review/proximity_feedback_test
/tmp/intelligent-cane-review/proximity_feedback_test
```
Observed output, exit status 0:
```text
PASS: boundaries, invalid ranges, monotonic feedback, polarity, beep transitions and rollover
```

## ESP32-C3 compile

Installed Arduino-ESP32 core: 3.3.2.
```sh
ARDUINO_BUILD_CACHE_PATH=/tmp/intelligent-cane-review/cache arduino-cli compile --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc,FlashMode=dio --build-path /tmp/intelligent-cane-review/build firmware/core-sensing
```
Observed output, exit status 0:
```text
Sketch uses 702734 bytes (53%) of program storage space. Maximum is 1310720 bytes.
Global variables use 24744 bytes (7%) of dynamic memory, leaving 302936 bytes for local variables. Maximum is 327680 bytes.
```

These are build and selected logic checks, not measurements of physical accuracy, runtime heap, response latency, BLE range, battery life or whole-system test coverage.
