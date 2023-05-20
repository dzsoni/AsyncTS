# AsyncTS

Asynchronous ThingSpeak Library for ESP8266 &amp; ESP32

## Why is it better than sychronous?

In synchronous mode, the program waits for the server to respond. It will only continue to run after the server replies. (Or max 5 sec if no response is received).
In asynchronous mode, while the server is thinking about the answer, your program can be busy with other tasks.

