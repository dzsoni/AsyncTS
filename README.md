# AsyncTS
Asynchronous ThingSpeak Library for ESP8266 &amp; ESP32

## Why is it better than sychronous?

In synchronous mode, your program will wait for a response from the server until the server responds. It will only continue to run after the server responds (or max 5 sec if no response is received).
