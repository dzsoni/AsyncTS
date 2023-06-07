# AsyncTS

Asynchronous ThingSpeak Library for ESP8266 &amp; ESP32.

## Why is it better than sychronous?

In synchronous mode, the program waits for the server to respond. It will only continue to run after the server replies. (Or max 5 sec if no response is received).
In asynchronous mode, while the server is thinking about the answer, your program can be busy with other tasks.

## Brief overview

The AsyncTS client can handle one request at a time. And until it finishes the previous one, it returns "false". So the new request has not been sent, if read or write functions return "false".
Before sending a request, you must set up a callback function to process server responses. The request type can be of two types, either it sends data to the server for storage or it requests to retrieve stored data.  
For write functions you have to define a callback function like this:

```c++
AsyncClient client;
AsyncTS ats;
void UserCallbackForAnyWriteFuncton(int code)
{
  //you can do something with return code.
  Serial.println("Server response:" + String(code));
}
int intvalue=4;//data for sending
ats.begin(client);
ats.onWriteServerResponseUserCB(UserCallbackForAnyWriteFuncton);
ats.writeField(channelID,field_num,intvalue,writeAPIkey);
```

This is true for all writing functions. Since the server returns only an error code of type int.  
For read functions you have to define a callback functions with an int parmeter for return code, and a `std::any*` for the server response. Inside the function, you must create a pointer to the type required by the function using `std::any_cast<T>`. If you cast to an incorrect type, you get a null pointer. Example:

```c++
AsyncClient client;
AsyncTS ats;
void IntResponse(int errorcode, std::any* resp)
{   
    //try any_cast<int>
    auto* a = std::any_cast<int>(resp);
    if (!a) 
    {
      // Type-mismatch
      Serial.println(" Bad_any_cast<int>");
      return;
    }
      Serial.println("Response code:" + String(code));
      Serial.println("Response: " + String(*a));//dereference pointer
}
...

ats.begin(client);
ats.onReadServerResponseUserCB(IntResponse);
ats.readIntField(channelID,field_num,readAPIkey);

```

## Note

To ESP32 platform I could only compile with  Visual Studio Code - PlatformIO IDE.
For success, put these lines in the platformio.ini file (below your ESP32 env. section).

```text
[env:ESP32]

build_flags =
    -std=gnu++17
build_unflags =
   -std=gnu++11

```
