/*
AsyncTS communication library for ESP8266 and ESP32.

MIT License

Copyright (c) 2023 János Füleki

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
-----------------------------------------------------------------------------
Based on:
mathworks/thingspeak-arduino,
me-no-dev/AsyncTCP and
boblemaire/asyncHTTPrequest libraries.
Bob Lemaire, IoTaWatt, Inc. xbuf

Required libraries:
https://github.com/me-no-dev/AsyncTCP
https://github.com/me-no-dev/ESPAsyncTCP
*/

#ifndef ASYNCTS_HPP
#define ASYNCTS_HPP

#include <any>
#include <pgmspace.h>
#include "Arduino.h"
#include "xbuf.h"

//#define DONT_COMPILE_DEBUG_LINES_AsyncTS


#ifndef DEBUG_IOTA_PORT
#define DEBUG_IOTA_PORT Serial
#endif

#ifdef ARDUINO_ARCH_ESP8266
#include <ESPAsyncTCP.h>
#ifndef SEMAPHORE_TAKE
#define SEMAPHORE_TAKE(X)
#endif
#ifndef SEMAPHORE_GIVE
#define SEMAPHORE_GIVE()
#endif
#endif

#ifdef ARDUINO_ARCH_ESP32
#include <AsyncTCP.h>
#ifndef SEMAPHORE_TAKE
#define SEMAPHORE_TAKE() xSemaphoreTake(_xSemaphore, portMAX_DELAY)
#endif
#ifndef SEMAPHORE_GIVE
#define SEMAPHORE_GIVE() xSemaphoreGive(_xSemaphore)
#endif
#endif

#ifdef DONT_COMPILE_DEBUG_LINES_AsyncTS
#define DEBUG_ATS(format, ...)
#else
#define DEBUG_ATS(format, ...)                                                 \
    if (_debug)                                                                \
    {                                                                          \
        DEBUG_IOTA_PORT.printf("Debug(%3ld): ", millis());                     \
        DEBUG_IOTA_PORT.printf_P(PSTR(format), ##__VA_ARGS__);                 \
    }
#endif

#define ATS_VER "0.1.1"

#define THINGSPEAK_URL "api.thingspeak.com"
#define THINGSPEAK_PORT_NUMBER 80
#define THINGSPEAK_HTTPS_PORT_NUMBER 443

#define DEFAULT_RX_TIMEOUT 30000


#ifdef ARDUINO_ARCH_ESP8266
#define TS_USER_AGENT "atslib-arduino/" ATS_VER " (ESP8266)"
#elif defined(ARDUINO_ARCH_ESP32)
#define TS_USER_AGENT "atslib-arduino/" ATS_VER " (ESP32)"
#else
#define TS_USER_AGENT "atslib-arduino/" ATS_VER " (unknown)"
#endif

#define FIELDNUM_MIN 1
#define FIELDNUM_MAX 8
#define FIELDLENGTH_MAX 255 // Max length for a field in ThingSpeak is 255 bytes (UTF-8)

#define TS_OK_SUCCESS 200               // OK / Success
#define TS_ERR_BADAPIKEY 400            // Incorrect API key (or invalid ThingSpeak server address)
#define TS_ERR_BADURL 404               // Incorrect API key (or invalid ThingSpeak server address)
#define TS_ERR_OUT_OF_RANGE -101        // Value is out of range or string is too long (> 255 bytes)
#define TS_ERR_INVALID_FIELD_NUM -201   // Invalid field number specified
#define TS_ERR_SETFIELD_NOT_CALLED -210 // setField() was not called before writeFields()
#define TS_ERR_CONNECT_FAILED -301      // Failed to connect to ThingSpeak
#define TS_ERR_UNEXPECTED_FAIL -302     // Unexpected failure during write to ThingSpeak
#define TS_ERR_BAD_RESPONSE -303        // Unable to parse response
#define TS_ERR_TIMEOUT -304             // Timeout waiting for server to respond
#define TS_ERR_NOT_INSERTED -401        // Point was not inserted (most probable cause is the rate limit of once every 15 seconds)

// variables to store the values from the readMultipleFields functionality
#ifndef ARDUINO_AVR_UNO
typedef struct feedRecord
{
    String nextReadField[8];
    String nextReadStatus;
    String nextReadLatitude;
    String nextReadLongitude;
    String nextReadElevation;
    String nextReadCreatedAt;
} feed;
#endif

/**
 * @typedef std::function<void (int responsecode)> writeResponseUserCB;
 * User's callback function for any 'write' function. writeField(),writeFields(),writeRaw().
 * @param responsecode Server response can be one of these values:
 * @arg  200      OK / Success
 * @arg  400      Incorrect API key (or invalid ThingSpeak server address)
 * @arg  404      Incorrect API key (or invalid ThingSpeak server address)
 * @arg -101      Value is out of range or string is too long (> 255 bytes)
 * @arg -201      Invalid field number specified
 * @arg -210      setField() was not called before writeFields()
 * @arg -301      Failed to connect to ThingSpeak
 * @arg -302      Unexpected failure during write to ThingSpeak
 * @arg -303      Unable to parse response
 * @arg -304      Timeout waiting for server to respond
 * @arg -401      Point was not inserted (most probable cause is the rate limit of once every 15 seconds)
*/
typedef std::function<void (int responsecode)> writeResponseUserCB;
/**
 * @typedef std::function<void (int responsecode, std::any* answare)> readResponseUserCB;
 * User's callback function for any 'read' function. 
 * readCreatedAt(),
 * readFloatField(),
 * readIntField(),
 * readLongField(),
 * readMultipleFields(),
 * readRaw(),
 * readStatus(),
 * readStringField().
 * @param responsecode Server response can be one of these values:
 * @arg  200  OK / Success
 * @arg  400  Incorrect API key (or invalid ThingSpeak server address)
 * @arg  404  Incorrect API key (or invalid ThingSpeak server address)
 * @arg -101  Value is out of range or string is too long (> 255 bytes)
 * @arg -201  Invalid field number specified
 * @arg -210  setField() was not called before writeFields()
 * @arg -301  Failed to connect to ThingSpeak
 * @arg -302  Unexpected failure during write to ThingSpeak
 * @arg -303  Unable to parse response
 * @arg -304  Timeout waiting for server to respond
 * @arg -401  Point was not inserted (most probable cause is the rate limit of once every 15 seconds)
 * @param answare A std::any* of type corresponding to the 'read' function.
*/
typedef std::function<void (int responsecode, std::any* answare)> readResponseUserCB;
typedef std::function<void ()> returnValueCB;


class AsyncTS
{
    private:
     enum clientstate{
                DISCONNECTED,
                CONNECTING,
                CONNECTED,
                HEADERSRCVD,
                RESONGOING,
                RESCOMPLETE,
                DISCONNECTING    
     } _state;

    AsyncClient*    _client;
    int             _lastTSerrorcode=TS_OK_SUCCESS; 
    bool            _debug = false;
    bool            _writesession;
    unsigned int    _port = THINGSPEAK_PORT_NUMBER;     
    size_t          _contentLength;                // content-length
    uint32_t        _timeout=DEFAULT_RX_TIMEOUT;   // Default or user overide RxTimeout in seconds
    uint32_t        _lastActivity;                 // Time of last activity

    writeResponseUserCB _writeResponseUserCB;
    returnValueCB       _retValueSelector;
    readResponseUserCB  _readResponseUserCB;

    String _nextWriteField[8];
    float _nextWriteLatitude;
    float _nextWriteLongitude;
    float _nextWriteElevation;
    String _nextWriteStatus;
    String _nextWriteTwitter;
    String _nextWriteTweet;
    String _nextWriteCreatedAt;

    xbuf       _request;                                       // Tx data buffer
    xbuf       _response;                                      // Rx data buffer

    int     _getWriteFieldsContentLength();
    int     _convertFloatToChar(float value, char *valueString);
    bool    _connectThingSpeak();
    bool    _writeHTTPHeader(const char * APIKey);
    void    _resetWriteFields();
    void    _setState(clientstate newState);
    void    _setPort(unsigned int port);
    bool    _readStringFieldInternal(unsigned long channelNumber, unsigned int field, const char * readAPIKey);
    float   _convertStringToFloat(String value);
    String  _getJSONValueByKey(String textToSearch, String key);
    String  _parseValues(String & multiContent, String key);
    unsigned int  _send();
    bool    _readRaw(unsigned long channelNumber, String suffixURL, const char * readAPIKey);
    bool    _isReady();
    
    

    void    _onConnect(AsyncClient*);
    void    _onDisconnect(AsyncClient*);
    void    _onData(void* Vbuf, size_t len);
    void    _onError(AsyncClient*, int8_t);
    void    _onPoll(AsyncClient*);
    void    _onAck( size_t len, uint32_t time);

    void    _readStringFieldCB();
    void    _readFloatFieldCB();
    void    _readLongFieldCB();
    void    _readIntFieldCB();
    void    _readCreatedAtCB();
    void    _readMultipleFieldsCB();
    void    _readStatusCB();
#if defined(ARDUINO_ARCH_ESP32)
  SemaphoreHandle_t _xSemaphore = nullptr;
#endif

public:
   
    feed lastFeed;
    
    AsyncTS();
    ~AsyncTS();
    bool begin                (AsyncClient& client );
    void setDebug             (bool debug);                     // Turn debug message on/off

    /** 
     * @brief Is debug ON or OFF?
     * @return True or false.
     */
    bool debug                (){ return _debug; };

    bool onWriteServerResponseUserCB(writeResponseUserCB wrucb);
    bool onReadServerResponseUserCB(readResponseUserCB reucb);

    /**
     * @brief Get last error code.
     * @returns 
     * @arg  200  OK / Success
     * @arg  400  Incorrect API key (or invalid ThingSpeak server address)
     * @arg  404  Incorrect API key (or invalid ThingSpeak server address)
     * @arg -101  Value is out of range or string is too long (> 255 bytes)
     * @arg -201  Invalid field number specified
     * @arg -210  setField() was not called before writeFields()
     * @arg -301  Failed to connect to ThingSpeak
     * @arg -302  Unexpected failure during write to ThingSpeak
     * @arg -303  Unable to parse response
     * @arg -304  Timeout waiting for server to respond
     * @arg -401  Point was not inserted (most probable cause is the rate limit of once every 15 seconds)
    */
    int getLastTSErrorCode(){return _lastTSerrorcode;};

    int setField(unsigned int field, int value);
    int setField(unsigned int field, long value);
    int setField(unsigned int field, float value);
    int setField(unsigned int field, String value);
    int setStatus(String status);
    int setLatitude(float latitude);
    int setLongitude(float longitude);
    int setElevation(float elevation);
    int setCreatedAt(String createdAt);
    int getFieldAsInt(unsigned int field);
    int setTwitterTweet(String twitter, String tweet);
    long getFieldAsLong(unsigned int field);
    void setTimeout(int milliseconds);           // Default or user overide RxTimeout in milliseconds
    void setClient(AsyncClient& client);
    float getFieldAsFloat(unsigned int field);
    String getFieldAsString(unsigned int field);
    
    String getStatus();
    String getLatitude();
    String getLongitude();
    String getElevation();
    String getCreatedAt();

    bool writeRaw(unsigned long channelNumber, String postMessage, const char *writeAPIKey);
    bool writeRaw(unsigned long channelNumber, String postMessage, const char *writeAPIKey, writeResponseUserCB wrucb);
    bool readRaw(unsigned long channelNumber, String suffixURL, const char * readAPIKey);
    bool readRaw(unsigned long channelNumber, String suffixURL, const char * readAPIKey, readResponseUserCB ruscb);

    bool writeField(unsigned long channelNumber, unsigned int field, String value, const char * writeAPIKey);
    bool writeField(unsigned long channelNumber, unsigned int field, String value, const char * writeAPIKey, writeResponseUserCB wrucb);
    bool writeField(unsigned long channelNumber, unsigned int field, int value, const char * writeAPIKey);
    bool writeField(unsigned long channelNumber, unsigned int field, int value, const char * writeAPIKey, writeResponseUserCB wrucb);
    bool writeField(unsigned long channelNumber, unsigned int field, long value, const char * writeAPIKey);
    bool writeField(unsigned long channelNumber, unsigned int field, long value, const char * writeAPIKey, writeResponseUserCB wrucb);
    bool writeField(unsigned long channelNumber, unsigned int field, float value, const char * writeAPIKey);
    bool writeField(unsigned long channelNumber, unsigned int field, float value, const char * writeAPIKey, writeResponseUserCB wrucb);
    bool writeFields(unsigned long channelNumber, const char * writeAPIKey);
    bool writeFields(unsigned long channelNumber, const char * writeAPIKey, writeResponseUserCB wrucb);

    bool readStringField(unsigned long channelNumber, unsigned int field, const char * readAPIKey);
    bool readStringField(unsigned long channelNumber, unsigned int field, const char * readAPIKey, readResponseUserCB ruscb);
    bool readStringField(unsigned long channelNumber, unsigned int field);
    bool readStringField(unsigned long channelNumber, unsigned int field , readResponseUserCB ruscb);
    bool readFloatField(unsigned long channelNumber, unsigned int field, const char * readAPIKey);
    bool readFloatField(unsigned long channelNumber, unsigned int field, const char * readAPIKey, readResponseUserCB ruscb);
    bool readFloatField(unsigned long channelNumber, unsigned int field);
    bool readFloatField(unsigned long channelNumber, unsigned int field, readResponseUserCB ruscb);
    bool readLongField(unsigned long channelNumber, unsigned int  field, const char * readAPIKey);
    bool readLongField(unsigned long channelNumber, unsigned int  field, const char * readAPIKey, readResponseUserCB ruscb);
    bool readLongField(unsigned long channelNumber, unsigned int field);
    bool readLongField(unsigned long channelNumber, unsigned int field, readResponseUserCB ruscb);
    bool readIntField(unsigned long channelNumber, unsigned int field, const char * readAPIKey);
    bool readIntField(unsigned long channelNumber, unsigned int field, const char * readAPIKey, readResponseUserCB ruscb);
    bool readIntField(unsigned long channelNumber, unsigned int field);
    bool readIntField(unsigned long channelNumber, unsigned int field, readResponseUserCB ruscb);

    bool readMultipleFields(unsigned long channelNumber, const char * readAPIKey);
    bool readMultipleFields(unsigned long channelNumber, const char * readAPIKey, readResponseUserCB ruscb);
    bool readMultipleFields(unsigned long channelNumber);
    bool readMultipleFields(unsigned long channelNumber, readResponseUserCB ruscb);

    bool readCreatedAt(unsigned long channelNumber, const char * readAPIKey);
    bool readCreatedAt(unsigned long channelNumber, const char * readAPIKey, readResponseUserCB ruscb);
    bool readCreatedAt(unsigned long channelNumber);
    bool readCreatedAt(unsigned long channelNumber, readResponseUserCB ruscb);

    bool readStatus(unsigned long channelNumber, const char * readAPIKey);
    bool readStatus(unsigned long channelNumber, const char * readAPIKey, readResponseUserCB ruscb);
    bool readStatus(unsigned long channelNumber);
    bool readStatus(unsigned long channelNumber, readResponseUserCB ruscb);

};
#endif /* ASYNCTS_HPP */
