#include "AsyncTS.hpp"

AsyncTS::AsyncTS()
{
    _resetWriteFields();
    _lastTSerrorcode = TS_OK_SUCCESS;
    #ifdef ARDUINO_ARCH_ESP32
    _xSemaphore = xSemaphoreCreateMutex();
    #endif
}
AsyncTS::~AsyncTS()
{
}

 
int AsyncTS::_getWriteFieldsContentLength()
{
    size_t iField;
    int contentLen = 0;

    for (iField = 0; iField < FIELDNUM_MAX; iField++)
    {
        if (this->_nextWriteField[iField].length() > 0)
        {
            contentLen = contentLen + 8 + this->_nextWriteField[iField].length(); // &fieldX=[value]

            // future-proof in case ThingSpeak allows 999 fields someday
            if (iField > 9)
            {
                contentLen = contentLen + 1;
            }
            else if (iField > 99)
            {
                contentLen = contentLen + 2;
            }
        }
    }

    if (!isnan(this->_nextWriteLatitude))
    {
        contentLen = contentLen + 5 + String(this->_nextWriteLatitude,6).length(); // &lat=[value]
    }

    if (!isnan(this->_nextWriteLongitude))
    {
        contentLen = contentLen + 6 + String(this->_nextWriteLongitude,6).length(); // &long=[value]
    }

    if (!isnan(this->_nextWriteElevation))
    {
        contentLen = contentLen + 11 + String(this->_nextWriteElevation,6).length(); // &elevation=[value]
    }

    if (this->_nextWriteStatus.length() > 0)
    {
        contentLen = contentLen + 8 + this->_nextWriteStatus.length(); // &status=[value]
    }

    if (this->_nextWriteTwitter.length() > 0)
    {
        contentLen = contentLen + 9 + this->_nextWriteTwitter.length(); // &twitter=[value]
    }

    if (this->_nextWriteTweet.length() > 0)
    {
        contentLen = contentLen + 7 + this->_nextWriteTweet.length(); // &tweet=[value]
    }

    if (this->_nextWriteCreatedAt.length() > 0)
    {
        contentLen = contentLen + 12 + this->_nextWriteCreatedAt.length(); // &created_at=[value]
    }

    if (contentLen == 0)
    {
        return 0;
    }

    contentLen = contentLen + 13; // add 14 for '&headers=false', subtract 1 for missing first '&'

    return contentLen;
}

int AsyncTS::_convertFloatToChar(float value, char *valueString)
{
    // Supported range is -999999000000 to 999999000000
    if (0 == isinf(value) && (value > 999999000000 || value < -999999000000))
    {
        // Out of range
        return TS_ERR_OUT_OF_RANGE;
    }
    // assume that 5 places right of decimal should be sufficient for most applications

#if defined(ARDUINO_ARCH_SAMD) || defined(ARDUINO_ARCH_SAM)
    sprintf(valueString, "%.5f", value);
#else
    dtostrf(value, 1, 5, valueString);
#endif
    _lastTSerrorcode = TS_OK_SUCCESS;
    return TS_OK_SUCCESS;
}

bool AsyncTS::_connectThingSpeak()
{
    DEBUG_ATS("ats::_connectThingSpeak() Connect to default ThingSpeak: %s:%u ...\r\n", THINGSPEAK_URL, _port);

    if (!_client->connected())
    {
        if (!_client->connect(THINGSPEAK_URL, _port))
        {
            DEBUG_ATS("!client.connect failed\r\n");
            _lastTSerrorcode = TS_ERR_CONNECT_FAILED;
            _setState(DISCONNECTED);
            return false;
        }
        _setState(CONNECTING);
    }
    else
    {
        _onConnect(_client);
    }
    _lastActivity = millis();
    return true;
}

bool AsyncTS::_writeHTTPHeader(const char *APIKey)
{
    _request.write("Host: api.thingspeak.com\r\n");
    _request.write("User-Agent: ");
    _request.write(TS_USER_AGENT);
    _request.write("\r\n");

    if (NULL != APIKey)
    {
        _request.write("X-THINGSPEAKAPIKEY: ");
        _request.write(APIKey);
        _request.write("\r\n");
    }
    return true;
}



void AsyncTS::_resetWriteFields()
{
    for (size_t iField = 0; iField < FIELDNUM_MAX; iField++)
    {
        _nextWriteField[iField] = "";
    }
    _nextWriteLatitude = NAN;
    _nextWriteLongitude = NAN;
    _nextWriteElevation = NAN;
    _nextWriteStatus = "";
    _nextWriteTwitter = "";
    _nextWriteTweet = "";
    _nextWriteCreatedAt = "";
}

void AsyncTS::_setState(clientstate newState)
{
    if (_state != newState)
    {
        _state = newState;
        DEBUG_ATS("ats::_setState(%d)\r\n", _state);
    }
}

void AsyncTS::_setPort(unsigned int port)
{
    _port = port;
}

bool AsyncTS::_readStringFieldInternal(unsigned long channelNumber, unsigned int field, const char *readAPIKey)
{
    DEBUG_ATS("ats::readStringFieldInternal(channelNumber: %lu  field: %i readAPIkey: %s)\r\n", channelNumber, field, readAPIKey);
    if (field < FIELDNUM_MIN || field > FIELDNUM_MAX)
    {
        _lastTSerrorcode = TS_ERR_INVALID_FIELD_NUM;
        _response.flush();
        if(_retValueSelector)
        {
            _retValueSelector();
        }
        return false;
    }
    String suffixURL = String("/fields/");
    suffixURL.concat(field);
    suffixURL.concat("/last");
    return _readRaw(channelNumber, suffixURL, readAPIKey);
}

float AsyncTS::_convertStringToFloat(String value)
{
    // There's a bug in the AVR function strtod that it doesn't decode -INF correctly (it maps it to INF)
    float result = value.toFloat();

    if (1 == isinf(result) && *value.c_str() == '-')
    {
        result = (float)-INFINITY;
    }

    return result;
}

String AsyncTS::_getJSONValueByKey(String textToSearch, String key)
{
    if (textToSearch.length() == 0)
    {
        return String("");
    }

    String searchPhrase = String("\"");
    searchPhrase.concat(key);
    searchPhrase.concat("\":\"");

    int fromPosition = textToSearch.indexOf(searchPhrase, 0);

    if (fromPosition == -1)
    {
        // return because there is no status or it's null
        return String("");
    }

    fromPosition = fromPosition + searchPhrase.length();

    int toPosition = textToSearch.indexOf("\"", fromPosition);

    if (toPosition == -1)
    {
        // return because there is no end quote
        return String("");
    }

    textToSearch.remove(toPosition);

    return textToSearch.substring(fromPosition);
}

String AsyncTS::_parseValues(String &multiContent, String key)
{
    if (multiContent.length() == 0)
    {
        return String("");
    }

    String searchPhrase = String("\"");
    searchPhrase.concat(key);
    searchPhrase.concat("\":\"");

    int fromPosition = multiContent.indexOf(searchPhrase, 0);

    if (fromPosition == -1)
    {
        // return because there is no status or it's null
        return String("");
    }

    fromPosition = fromPosition + searchPhrase.length();

    int toPosition = multiContent.indexOf("\"", fromPosition);

    if (toPosition == -1)
    {
        // return because there is no end quote
        return String("");
    }

    return multiContent.substring(fromPosition, toPosition);
}


unsigned int AsyncTS::_send()
{
    if (_request.available() == 0)
        return 0;
    if( ! _client->connected())
    {
        _timeout =DEFAULT_RX_TIMEOUT;
        return 0;
    }
    else if( ! _client->canSend())
    {
        return 0;
    }
    DEBUG_ATS("_send() %d\r\n", _request.available());

    size_t supply = _request.available();
    size_t demand = _client->space();
    if (supply > demand)
        supply = demand;
    size_t sent = 0;
    uint8_t *temp = new uint8_t[100];
    if (!temp) return 0;

    while (supply)
    {
        size_t chunk = supply < 100 ? supply : 100;
        supply -= _request.read(temp, chunk);
        sent += _client->add((char *)temp, chunk);
    }
    delete temp;

    _client->send();
    DEBUG_ATS("*sent %d\r\n", sent);
    _lastActivity = millis();
    return sent;
}

void AsyncTS::_onConnect(AsyncClient *client)
{
    DEBUG_ATS("ats::_onConnect handle \r\n");
    SEMAPHORE_TAKE();
    _setState(CONNECTED);
    //_response = new xbuf;
    _contentLength = 0;
    //_chunked = false;
    _client->onAck([](void *obj, AsyncClient *client, size_t len, uint32_t time)
                   { ((AsyncTS *)(obj))->_onAck(len, time); },
                   this);
    _client->onData([](void *obj, AsyncClient *client, void *data, size_t len)
                    { ((AsyncTS *)(obj))->_onData(data, len); },
                    this);
    if (_client->canSend())
    {
        _send();
    }
    _lastActivity = millis();
    SEMAPHORE_GIVE();
}

void AsyncTS::_onDisconnect(AsyncClient *client)
{
    DEBUG_ATS("ats::_onDisconnect\r\n")
    _setState(DISCONNECTED);
}


void AsyncTS::_onData(void *Vbuf, size_t len)
{
    DEBUG_ATS("_onData handler %.16s... (%d)\r\n", (char *)Vbuf, len);
    SEMAPHORE_TAKE();
    _lastActivity = millis();

    // Transfer data to xbuf

    _response.write((uint8_t *)Vbuf, len);

    // if header not complete, collect it.
    // if still not complete, just return.

    while (_state == CONNECTED)
    {
        String headerLine = _response.readStringUntil("\r\n");

        // If no line, return.

        if (!headerLine.length())
        {
            _lastTSerrorcode = TS_ERR_BAD_RESPONSE;
            SEMAPHORE_GIVE();
            return;
        }

        // If empty line, all headers are in, advance readyState.

        if (headerLine.length() == 2)
        {
            _setState(HEADERSRCVD);
        }

        // If line is HTTP header, capture HTTPcode.

        else if (headerLine.substring(0, 8) == "HTTP/1.1")
        {
            DEBUG_ATS("HTTP/1.1 found.\r\n");
            _lastTSerrorcode = headerLine.substring(9, headerLine.indexOf(' ', 9)).toInt();
            DEBUG_ATS("Response code: %d\r\n", _lastTSerrorcode);
        }

        else if (headerLine.substring(0, 15) == "Content-Length:")
        {
            _contentLength = headerLine.substring(16, headerLine.indexOf(' ', 16)).toInt();
            DEBUG_ATS("Content-Length :%d\r\n", _contentLength);
        }
    }


    if (_response.available() && _state != RESCOMPLETE)
    {
        _setState(RESONGOING);
    }

    // If not chunked and all data read, close it up.

    if (_response.available() >= _contentLength)
    {
        if (_writesession)
        {
            if (_response.readString().toInt() == 0)
            {
                _lastTSerrorcode = TS_ERR_NOT_INSERTED;
            }
            if (_writeResponseUserCB)
                _writeResponseUserCB(_lastTSerrorcode);
        }
        else
        {
            if (_retValueSelector)
                _retValueSelector();
        }
        _setState(RESCOMPLETE);
        _setState(DISCONNECTING);
        _client->stop();
        
    }
    SEMAPHORE_GIVE();
}

void AsyncTS::_onError(AsyncClient *client, int8_t error)
{
    DEBUG_ATS("ats::_onError:%d\r\n", error);
}

void AsyncTS::_onPoll(AsyncClient *client)
{
    
}

void AsyncTS::_onAck(size_t len, uint32_t time)
{
    _send();
}

/**
 * @brief Initializes the ThingSpeak library and network settings using the ThingSpeak.com service.
 * @param  &client AsynClient reference. Created earlier in the sketch.
 * @return Always true.
 * 
 * This does not validate the information passed in, or generate any calls to ThingSpeak.
*/
bool AsyncTS::begin(AsyncClient &client)
{
    DEBUG_ATS("ats::begin\r\n");
    _setPort(THINGSPEAK_PORT_NUMBER);
    _client = &client;
    _resetWriteFields();
    _setState(DISCONNECTED);
    _client->setRxTimeout(_timeout/1000);//convert to sec
    _client->setAckTimeout(_timeout);
    _client->onConnect([](void *obj, AsyncClient *client)
                       { ((AsyncTS *)(obj))->_onConnect(client); },
                       this);
    _client->onDisconnect([](void *obj, AsyncClient *client)
                          { ((AsyncTS *)(obj))->_onDisconnect(client); },
                          this);
    _client->onPoll([](void *obj, AsyncClient *client)
                    { ((AsyncTS *)(obj))->_onPoll(client); },
                    this);
    _client->onError([](void *obj, AsyncClient *client, uint32_t error)
                     { ((AsyncTS *)(obj))->_onError(client, error); },
                     this);
    return true;
  
}

/**
 * @brief Set no RX data timeout for the connection and no ACK timeout for the last sent packet in milliseconds
 * @param milliseconds timeout value in milliseconds.
*/
void AsyncTS::setTimeout(int milliseconds)
{
    DEBUG_ATS("setTimeout(%d)\r\n", milliseconds);
    if(_client)
    {
        _client->setRxTimeout(milliseconds/1000);// covert to sec
        _client->setAckTimeout(milliseconds);
    }
    _timeout = milliseconds;
}

/**
 * @brief Write a raw POST to a ThingSpeak channel
 * @pre Call onWriteServerResponseUserCB() before.
 * @param channelNumber Thingspeak channel number
 * @param postMessage   Raw URL to write to ThingSpeak as a string.  See the documentation at https://thingspeak.com/docs/channels#update_feed.
 * @param writeAPIKey   WriteAPIkey for your channel  *If you share code with others, do _not_ share this key*
 * @retval false: AsyncTS client is busy. Couldn't send the request.
 * @retval true: request is under sending.
*/
bool AsyncTS::writeRaw(unsigned long channelNumber, String postMessage, const char *writeAPIKey)
{
    _lastTSerrorcode=TS_OK_SUCCESS;
    DEBUG_ATS("ats::writeRaw(channelNumber:%lu message:%s writeAPIKey: %s)\r\n", channelNumber, postMessage, writeAPIKey);
    if (!_isReady())
    {
        DEBUG_ATS("ats::writeRaw Clinet is busy.");
        _lastTSerrorcode = TS_ERR_CONNECT_FAILED;
        return false;
    }
    _response.flush();
    _request.flush();
    _writesession = true;

    postMessage.concat("&headers=false");

    DEBUG_ATS("POST \"%s\"\r\n", postMessage.c_str());

    // Post data to thingspeak

    _request.write("POST /update HTTP/1.1\r\n");
    _writeHTTPHeader(writeAPIKey);
    _request.write("Content-Type: application/x-www-form-urlencoded\r\n");
    _request.write("Content-Length: ");
    _request.write(String(postMessage.length()));
    _request.write("\r\n\r\n");
    _request.write(postMessage);

    if (!_connectThingSpeak())
        return false;
    //_state = CONNECTING
    return true;
}

/**
 * @brief Write a raw POST to a ThingSpeak channel
 * @param channelNumber Thingspeak channel number
 * @param postMessage   Raw URL to write to ThingSpeak as a string.  See the documentation at https://thingspeak.com/docs/channels#update_feed.
 * @param writeAPIKey   WriteAPIkey for your channel  *If you share code with others, do _not_ share this key*
 * @param wrucb         User's callback function to process the answare of server.
 * @retval false: AsyncTS client is busy. Couldn't send the request.
 * @retval true: request is under sending.
*/
bool AsyncTS::writeRaw(unsigned long channelNumber, String postMessage, const char *writeAPIKey, writeResponseUserCB wrucb)
{
    if (!_isReady())
    {
        DEBUG_ATS("ats::writeRaw Clinet is busy.");
        _lastTSerrorcode = TS_ERR_CONNECT_FAILED;
        return false;
    }
    _writeResponseUserCB = wrucb;
    return writeRaw(channelNumber,postMessage,writeAPIKey);
}

/**
  * @brief Read a raw response from a private ThingSpeak channel
  * @pre Call onReadServerResponseUserCB() before.
  * @post User's callback need process std::any<String>*.
  * @param channelNumber Channnel number
  * @param suffixURL Raw URL to write to ThingSpeak as a String.  See the documentation at https://thingspeak.com/docs/channels#get_feed
  * @param readAPIKey Read API key associated with the channel.  *If you share code with others, do _not_ share this key*
  * @retval false: AsyncTS client is busy. Couldn't send the request.
  * @retval true: request is under sending.
 */
bool AsyncTS::_readRaw(unsigned long channelNumber, String suffixURL, const char *readAPIKey)
{
    DEBUG_ATS("ats::readRaw (channelNumber: %lu  readAPIkey: %s suffixURL: \"%s\r\n", channelNumber, readAPIKey, suffixURL.c_str());
    _lastTSerrorcode=TS_OK_SUCCESS;
    if (!_isReady())
    {
        DEBUG_ATS("ats::readRaw Clinet is busy.");
        _lastTSerrorcode = TS_ERR_CONNECT_FAILED;
        return false;
    }
    _writesession = false;
    _response.flush();
    _request.flush();

    String readURL = String("/channels/");
    readURL.concat(channelNumber);
    readURL.concat(suffixURL);
    DEBUG_ATS("ats::readRaw GET \"%s\"\r\n", readURL);

    // Get data from thingspeak
    _request.write("GET ");
    _request.write(readURL);
    _request.write(" HTTP/1.1\r\n");
    _writeHTTPHeader(readAPIKey);
    _request.write("\r\n");

    if (!_connectThingSpeak())return false;
    //_state = CONNECTING
    return true;
}

bool AsyncTS::_isReady()
{
    return (_state==DISCONNECTED);
}

/**
  * @brief Read a raw response from a private ThingSpeak channel
  * @pre Call onReadServerResponseUserCB() before.
  * @post User's callback need process std::any<String>*.
  * @param channelNumber Channnel number
  * @param suffixURL Raw URL to write to ThingSpeak as a String.  See the documentation at https://thingspeak.com/docs/channels#get_feed
  * @param readAPIKey Read API key associated with the channel.  *If you share code with others, do _not_ share this key*
  * @retval false: AsyncTS client is busy. Couldn't send the request.
  * @retval true: request is under sending.
 */
bool AsyncTS::readRaw(unsigned long channelNumber, String suffixURL, const char * readAPIKey)
{
    if (!_isReady())
    {
        DEBUG_ATS("ats::writeRaw Clinet is busy.");
        _lastTSerrorcode = TS_ERR_CONNECT_FAILED;
        return false;
    }
    _retValueSelector = [this](){ this->_readStringFieldCB(); };
    return _readRaw(channelNumber,suffixURL,readAPIKey);
}
/**
  * @brief Read a raw response from a private ThingSpeak channel
  * @post User's callback need process std::any<String>*.
  * @param channelNumber Channnel number
  * @param suffixURL Raw URL to write to ThingSpeak as a String.  See the documentation at https://thingspeak.com/docs/channels#get_feed
  * @param readAPIKey Read API key associated with the channel.  *If you share code with others, do _not_ share this key*
  * @param ruscb User's callback function to process the server response.
  * @return If false , client is busy or can't connect.
 */
bool AsyncTS::readRaw(unsigned long channelNumber, String suffixURL, const char * readAPIKey, readResponseUserCB ruscb)
{
    if (!_isReady())
    {
        DEBUG_ATS("ats::writeRaw Clinet is busy.");
        _lastTSerrorcode = TS_ERR_CONNECT_FAILED;
        return false;
    }
    _readResponseUserCB = ruscb;
    _retValueSelector = [this](){ this->_readStringFieldCB(); };
    return _readRaw(channelNumber,suffixURL,readAPIKey);
}


void AsyncTS::_readCreatedAtCB()
{
    if (_readResponseUserCB)
    {
        std::any res = String("");
        if (_lastTSerrorcode == TS_OK_SUCCESS)
        {
            res = _getJSONValueByKey(_response.readString(), "created_at");
        }

        _readResponseUserCB(_lastTSerrorcode, &res);
    }
}
/**
 * @brief Read the created-at timestamp associated with the latest update to a private ThingSpeak channel
 * 
 * @pre Call onReadServerResponseUserCB() before.
 * @post User's callback need process std::any<String>*.
 * 
 * @param channelNumber Channel number
 * @param readAPIKey Read API key associated with the channel.  *If you share code with others, do _not_ share this key*
 * @retval false: AsyncTS client is busy. Couldn't send the request.
 * @retval true: request is under sending.
*/
bool AsyncTS::readCreatedAt(unsigned long channelNumber, const char *readAPIKey)
{
    if (!_isReady())
    {
        DEBUG_ATS("ats::writeRaw Clinet is busy.");
        _lastTSerrorcode = TS_ERR_CONNECT_FAILED;
        return false;
    }
    _retValueSelector = [this](){ this->_readCreatedAtCB();};
    _request.flush();
    _response.flush();
    return _readRaw(channelNumber, "/feeds/last.txt", readAPIKey);
}

/**
 * @brief Read the created-at timestamp associated with the latest update to a private ThingSpeak channel
 * 
 * @param channelNumber Channel number
 * @param readAPIKey Read API key associated with the channel.  *If you share code with others, do _not_ share this key*
 * @param ruscb User's callback function to process the server response.
 * @retval false: AsyncTS client is busy. Couldn't send the request.
 * @retval true: request is under sending.
 * @post User's callback need process std::any<String>*.
*/
bool AsyncTS::readCreatedAt(unsigned long channelNumber, const char * readAPIKey, readResponseUserCB ruscb)
{
    if (!_isReady())
    {
        DEBUG_ATS("ats::writeRaw Clinet is busy.");
        _lastTSerrorcode = TS_ERR_CONNECT_FAILED;
        return false;
    }
    _readResponseUserCB = ruscb;
    return readCreatedAt(channelNumber,readAPIKey);
}


/**
 * @brief Read the created-at timestamp associated with the latest update to a public ThingSpeak channel
 * 
 * @pre Call onReadServerResponseUserCB() before.
 * 
 * @param channelNumber Channel number
 * @retval false: AsyncTS client is busy. Couldn't send the request.
 * @retval true: request is under sending.
 * @post User's callback need process std::any<String>*.
*/
bool AsyncTS::readCreatedAt(unsigned long channelNumber)
{
    return readCreatedAt(channelNumber, NULL);
}

/**
 * @brief Read the created-at timestamp associated with the latest update to a public ThingSpeak channel
 * 
 * @param channelNumber Channel number
 * @param ruscb User's callback function to process the server response.
 * @retval false: AsyncTS client is busy. Couldn't send the request.
 * @retval true: request is under sending.
 * @post User's callback need process std::any<String>*.
*/
bool AsyncTS::readCreatedAt(unsigned long channelNumber, readResponseUserCB ruscb)
{
    if (!_isReady())
    {
        DEBUG_ATS("ats::writeRaw Clinet is busy.");
        _lastTSerrorcode = TS_ERR_CONNECT_FAILED;
        return false;
    }
    _readResponseUserCB = ruscb;
    return readCreatedAt(channelNumber,NULL);
}
/**
 * @brief  Set AsyncClient.
 * @param  &client AsynClient reference.
*/
void AsyncTS::setClient(AsyncClient &client)
{
    _client = &client;
}
/**
 * @brief Set on or off the bebug messages.
 * @param debug true/on , false/off
 * @note If you define the option "DONT_COMPILE_DEBUG_LINES_AsyncTS" in the Async.hpp file,
 * you can save extra flash memory, because the source file will not contain the error messages.
*/
void AsyncTS::setDebug(bool debug)
{
    if (_debug || debug)
    {
        DEBUG_ATS("ats::setDebug(%s) \r\n", debug ? "on" : "off");

#ifdef DONT_COMPILE_DEBUG_LINES_AsyncTS
        if (debug)
        {
            DEBUG_IOTA_PORT.printf("Sourcecode doesn't contain debug lines.");
        }
#endif
    }
    _debug = debug;
}

/**
 * @brief Set the callback function to process the server reponse. The user's function is called after these
 * 'write' funtions: writeField(),writeFields(),writeRaw()
 * @param wrucb User's callback function.
 * @return False if couldn't set the callback function.
*/
bool AsyncTS::onWriteServerResponseUserCB(writeResponseUserCB wrucb)
{
  if (!_isReady())
    {
        DEBUG_ATS("ats::onWriteServerResponseUserCB: Client is busy.");
        return false;
    }
    _writeResponseUserCB = wrucb;
    return true;
}

/**
* @brief Set the callback function to process the server reponse. The user's function is called after these
* 'read' funtions: readCreatedAt(), readFloatField(), readIntField(), readLongField(), 
* readMultipleFields(), readRaw(), readStatus(), readStringField().
* @param reucb User's callback function.
* @return False if couldn't set the callback function.
*/
bool AsyncTS::onReadServerResponseUserCB(readResponseUserCB reucb)
{
    if (!_isReady())
    {
        DEBUG_ATS("ats::onReadServerResponseUserCB: Client is busy.");
        return false;
    }
    _readResponseUserCB = reucb;
    return true;
}

/**
 * @brief Write a String value to a single field in a ThingSpeak channel.
 * @pre Call onWriteServerResponseUserCB() to set the user's callback function.
 * @param   channelNumber   Channel number.
 * @param   field           Field number (1-8) within the channel to write to.
 * @param   value           String value.
 * @param   writeAPIKey     Write API key associated with the channel.  *If you share code with others, do _not_ share this key*
 * @retval false: AsyncTS client is busy. Couldn't send the request.
 * @retval true: request is under sending.
*/
bool AsyncTS::writeField(unsigned long channelNumber, unsigned int field, String value, const char *writeAPIKey)
{
    _lastTSerrorcode=TS_OK_SUCCESS;
    DEBUG_ATS("ats::writeField begin \r\n")
   if (!_isReady())
    {
        DEBUG_ATS("ats::writeRaw Clinet is busy.");
        _lastTSerrorcode = TS_ERR_CONNECT_FAILED;
        return false;
    }
    // Invalid field number specified
    if (field < FIELDNUM_MIN || field > FIELDNUM_MAX)
    {
        _lastTSerrorcode = TS_ERR_INVALID_FIELD_NUM;
        if (_writeResponseUserCB)
        {
            _writeResponseUserCB(_lastTSerrorcode);
        }
        return false;
    }

    // Max # bytes for ThingSpeak field is 255
    if (value.length() > FIELDLENGTH_MAX)
    {
        _lastTSerrorcode = TS_ERR_OUT_OF_RANGE;
        if (_writeResponseUserCB)
        {
            _writeResponseUserCB(_lastTSerrorcode);
        }
        return false;
    }

    DEBUG_ATS("ats::writeField (channelNumber: %lu  field: %i writeAPIkey: %s )\r\n", channelNumber, field, writeAPIKey);

    String postMessage = String("field");
    postMessage.concat(field);
    postMessage.concat("=");
    postMessage.concat(value);

    return writeRaw(channelNumber, postMessage, writeAPIKey);
}
/**
 * @brief Write a String value to a single field in a ThingSpeak channel
 * 
 * @param   channelNumber   Channel number.
 * @param   field           Field number (1-8) within the channel to write to.
 * @param   value           String value.
 * @param   writeAPIKey     Write API key associated with the channel.  *If you share code with others, do _not_ share this key*
 * @param   wrucb           User's callback function to process the server response.
 * @retval false: AsyncTS client is busy. Couldn't send the request.
 * @retval true: request is under sending.
*/
bool AsyncTS::writeField(unsigned long channelNumber, unsigned int field, String value, const char * writeAPIKey, writeResponseUserCB wrucb)
{
    if (!_isReady())
    {
        DEBUG_ATS("ats::writeRaw Clinet is busy.");
        _lastTSerrorcode = TS_ERR_CONNECT_FAILED;
        return false;
    }
    _writeResponseUserCB = wrucb;
    return writeField(channelNumber,field,value,writeAPIKey);
}

/**
 * @brief Write an integer value to a single field in a ThingSpeak channel
 * 
 * @pre Call onWriteServerResponseUserCB() to set the user's callback function.
 * 
 * @param   channelNumber   Channel number
 * @param   field           Field number (1-8) within the channel to write to.
 * @param   value           Integer value (from -32,768 to 32,767) to write.
 * @param   writeAPIKey     Write API key associated with the channel.  *If you share code with others, do _not_ share this key*
 * @retval false: AsyncTS client is busy. Couldn't send the request.
 * @retval true: request is under sending.
*/
bool AsyncTS::writeField(unsigned long channelNumber, unsigned int field, int value, const char *writeAPIKey)
{   
    if (!_isReady())
    {
        DEBUG_ATS("ats::writeRaw Clinet is busy.");
        _lastTSerrorcode = TS_ERR_CONNECT_FAILED;
        return false;
    }
    char valueString[10]; // int range is -32768 to 32768, so 7 bytes including terminator, plus a little extra
    itoa(value, valueString, 10);
    return writeField(channelNumber, field, valueString, writeAPIKey);
}

/**
 * @brief Write an integer value to a single field in a ThingSpeak channel
 * @param   channelNumber   Channel number
 * @param   field           Field number (1-8) within the channel to write to.
 * @param   value           Integer value (from -32,768 to 32,767) to write.
 * @param   writeAPIKey     Write API key associated with the channel.  *If you share code with others, do _not_ share this key*
 * @param   wrucb           User's callback function to process the server response.
 * @retval false: AsyncTS client is busy. Couldn't send the request.
 * @retval true: request is under sending.
*/
bool AsyncTS::writeField(unsigned long channelNumber, unsigned int field, int value, const char * writeAPIKey, writeResponseUserCB wrucb)
{
    if (!_isReady())
    {
        DEBUG_ATS("ats::writeRaw Clinet is busy.");
        _lastTSerrorcode = TS_ERR_CONNECT_FAILED;
        return false;
    }
    _writeResponseUserCB = wrucb;
    return writeField(channelNumber, field, value, writeAPIKey);
}

/**
 * @brief Write a long value to a single field in a ThingSpeak channel
 * 
 * @pre Call onWriteServerResponseUserCB() to set the user's callback function.
 * 
 * @param   channelNumber   Channel number
 * @param   field           Field number (1-8) within the channel to write to.
 * @param   value           Long value (from -2,147,483,648 to 2,147,483,647) to write.
 * @param   writeAPIKey     Write API key associated with the channel.  *If you share code with others, do _not_ share this key*
 * @retval false: AsyncTS client is busy. Couldn't send the request.
 * @retval true: request is under sending.
*/
bool AsyncTS::writeField(unsigned long channelNumber, unsigned int field, long value, const char *writeAPIKey)
{
    if (!_isReady())
    {
        DEBUG_ATS("ats::writeRaw Clinet is busy.");
        _lastTSerrorcode = TS_ERR_CONNECT_FAILED;
        return false;
    }
    char valueString[15]; // long range is -2147483648 to 2147483647, so 12 bytes including terminator
    ltoa(value, valueString, 10);
    return writeField(channelNumber, field, valueString, writeAPIKey);
}

/**
 * @brief Write a long value to a single field in a ThingSpeak channel
 * @param   channelNumber   Channel number
 * @param   field           Field number (1-8) within the channel to write to.
 * @param   value           Long value (from -2,147,483,648 to 2,147,483,647) to write.
 * @param   writeAPIKey     Write API key associated with the channel.  *If you share code with others, do _not_ share this key*
 * @param   wrucb           User's callback function to process the server response.
 * @retval false: AsyncTS client is busy. Couldn't send the request.
 * @retval true: request is under sending.
*/
bool AsyncTS::writeField(unsigned long channelNumber, unsigned int field, long value, const char * writeAPIKey, writeResponseUserCB wrucb)
{
    if (!_isReady())
    {
        DEBUG_ATS("ats::writeRaw Clinet is busy.");
        _lastTSerrorcode = TS_ERR_CONNECT_FAILED;
        return false;
    }
    _writeResponseUserCB = wrucb;
    return writeField(channelNumber, field, value, writeAPIKey);
}

/**
 * @brief Write a floating point value to a single field in a ThingSpeak channel
 * 
 * @pre Call onWriteServerResponseUserCB() to set the user's callback function.
 * 
 * @param   channelNumber   Channel number
 * @param   field           Field number (1-8) within the channel to write to.
 * @param   value           Floating point value (from -999999000000 to 999999000000) to write.  If you need more accuracy, or a wider range, you should format the number using <tt>dtostrf</tt> and writeField().
 * @param   writeAPIKey     Write API key associated with the channel.  *If you share code with others, do _not_ share this key*
 * @retval false: AsyncTS client is busy. Couldn't send the request.
 * @retval true: request is under sending.
*/
bool AsyncTS::writeField(unsigned long channelNumber, unsigned int field, float value, const char *writeAPIKey)
{
    if (!_isReady())
    {
        DEBUG_ATS("ats::writeRaw Clinet is busy.");
        _lastTSerrorcode = TS_ERR_CONNECT_FAILED;
        return false;
    }
    char valueString[20]; // range is -999999000000.00000 to 999999000000.00000, so 19 + 1 for the terminator
    int status = _convertFloatToChar(value, valueString);
    if (status != TS_OK_SUCCESS)
    {
        _lastTSerrorcode = status;
        if (_writeResponseUserCB)
            _writeResponseUserCB(_lastTSerrorcode);
        return false;
    }
    return writeField(channelNumber, field, valueString, writeAPIKey);
}

/**
 * @brief Write a floating point value to a single field in a ThingSpeak channel
 * @param   channelNumber   Channel number
 * @param   field           Field number (1-8) within the channel to write to.
 * @param   value           Floating point value (from -999999000000 to 999999000000) to write.  If you need more accuracy, or a wider range, you should format the number using <tt>dtostrf</tt> and writeField().
 * @param   writeAPIKey     Write API key associated with the channel.  *If you share code with others, do _not_ share this key*
 * @param   wrucb           User's callback function to process the server response.
 * @retval false: AsyncTS client is busy. Couldn't send the request.
 * @retval true: request is under sending.
*/
bool AsyncTS::writeField(unsigned long channelNumber, unsigned int field, float value, const char * writeAPIKey, writeResponseUserCB wrucb)
{
    if (!_isReady())
    {
        DEBUG_ATS("ats::writeRaw Clinet is busy.");
        _lastTSerrorcode = TS_ERR_CONNECT_FAILED;
        return false;
    }
    _writeResponseUserCB = wrucb;
    return writeField(channelNumber, field, value, writeAPIKey);
}

/**
 * @brief    Write a multi-field update.
 * 
 * Call  onWriteServerResponseUserCB(),setField(), setLatitude(), setLongitude(), setElevation()
 * and/or setStatus() and then call writeFields()
 * 
 * @param    channelNumber Channel number
 * @param    writeAPIKey Write API key associated with the channel.  *If you share code with others, do _not_ share this key*
 * @retval false: AsyncTS client is busy. Couldn't send the request.
 * @retval true: request is under sending. 
*/
bool AsyncTS::writeFields(unsigned long channelNumber, const char *writeAPIKey)
{
    if (!_isReady())
    {
        DEBUG_ATS("ats::writeRaw Clinet is busy.");
        _lastTSerrorcode = TS_ERR_CONNECT_FAILED;
        return false;
    }
    _response.flush();
    _request.flush();
    _writesession = true;
    // Get the content length of the payload
    int contentLen = _getWriteFieldsContentLength();
   
    if (contentLen == 0)
    {
        // setField was not called before writeFields
        _lastTSerrorcode = TS_ERR_SETFIELD_NOT_CALLED;
        if (_writeResponseUserCB)
        {
            _writeResponseUserCB(_lastTSerrorcode);
        }
        return false;
    }
    _request.write("POST /update HTTP/1.1\r\n");
    _writeHTTPHeader(writeAPIKey);
    _request.write("Content-Type: application/x-www-form-urlencoded\r\n");
    _request.write("Content-Length: ");
    _request.write(String(contentLen));
    _request.write("\r\n\r\n");

    bool fFirstItem = true;
    for (size_t iField = 0; iField < FIELDNUM_MAX; iField++)
    {
        if (this->_nextWriteField[iField].length() > 0)
        {
            if (!fFirstItem)
            {
                _request.write("&");
            }
            _request.write("field");
            _request.write(String(iField + 1));
            _request.write("=");
            _request.write(_nextWriteField[iField]);
            fFirstItem = false;
        }
    }

    if (!isnan(this->_nextWriteLatitude))
    {
        if (!fFirstItem)
        {
            _request.write("&");
        }
        _request.write("lat=");
        _request.write(String(_nextWriteLatitude,6));
        fFirstItem = false;
    }

    if (!isnan(this->_nextWriteLongitude))
    {
        if (!fFirstItem)
        {
             _request.write("&");
        }
        _request.write("long=");
        _request.write(String(_nextWriteLongitude,6));
        fFirstItem = false;
    }

    if (!isnan(_nextWriteElevation))
    {
        if (!fFirstItem)
        {
            _request.write("&");
        }
        _request.write("elevation=");
        _request.write(String(_nextWriteElevation,6));
        fFirstItem = false;
    }

    if (_nextWriteStatus.length() > 0)
    {
        if (!fFirstItem)
        {
            _request.write("&");    
        }
        _request.write("status=");
        _request.write(_nextWriteStatus);
        fFirstItem = false;
    }

    if (_nextWriteTwitter.length() > 0)
    {
        if (!fFirstItem)
        {
            _request.write("&");
        }
        _request.write("twitter=");
        _request.write(_nextWriteTwitter);
        fFirstItem = false;
    }

    if (_nextWriteTweet.length() > 0)
    {
        if (!fFirstItem)
        {
            _request.write("&");
        }
        _request.write("tweet=");
        _request.write(this->_nextWriteTweet);
        fFirstItem = false;
    }

    if (_nextWriteCreatedAt.length() > 0)
    {
        if (!fFirstItem)
        {
            _request.write("&");
        }
        _request.write("created_at=");
        _request.write(_nextWriteCreatedAt);
        fFirstItem = false;
    }

     _request.write("&headers=false");

    _resetWriteFields();
   if (!_connectThingSpeak())
        return false;
    //_state = CONNECTING
    return true;
}

/**
  *@brief Write a multi-field update.
  *@pre Call setField(), setLatitude(), setLongitude(), setElevation() and/or setStatus() and then call writeFields().
  *@param channelNumber Channel number
  *@param writeAPIKey   Write API key associated with the channel.  *If you share code with others, do _not_ share this key*
  *@param wrucb         User's callback function to process the server response.
 * @retval false: AsyncTS client is busy. Couldn't send the request.
 * @retval true: request is under sending.
*/
bool AsyncTS::writeFields(unsigned long channelNumber, const char * writeAPIKey, writeResponseUserCB wrucb)
{
    if (!_isReady())
    {
        DEBUG_ATS("ats::writeRaw Clinet is busy.");
        _lastTSerrorcode = TS_ERR_CONNECT_FAILED;
        return false;
    }
    _writeResponseUserCB = wrucb;
    return writeFields(channelNumber, writeAPIKey);
}

void AsyncTS::_readStringFieldCB()
{
    if (_readResponseUserCB)
    {
        std::any a = _response.readString();
        _readResponseUserCB(_lastTSerrorcode, &a);
    }
}

/**
 * @brief Read the latest string from a private ThingSpeak channel.
 * 
 * @pre First call  onReadServerResponseUserCB().
 * @post Through user' callback: std::any<string>* need process.
 * @param channelNumber Channel number
 * @param field Field number (1-8) within the channel to read from.
 * @param readAPIKey Read API key associated with the channel.  *If you share code with others, do _not_ share this key*
 * @retval false: AsyncTS client is busy. Couldn't send the request.
 * @retval true: request is under sending.
*/
bool AsyncTS::readStringField(unsigned long channelNumber, unsigned int field, const char *readAPIKey)
{
    if (!_isReady())
    {
        DEBUG_ATS("ats::writeRaw Clinet is busy.");
        _lastTSerrorcode = TS_ERR_CONNECT_FAILED;
        return false;
    }
    _retValueSelector = [this]()
    { this->_readStringFieldCB(); };
    return _readStringFieldInternal(channelNumber, field, readAPIKey);
}

/**
 * @brief Read the latest string from a private ThingSpeak channel.
 * @param channelNumber Channel number
 * @post Through user' callback: std::any<string>* need process.
 * @param field Field number (1-8) within the channel to read from.
 * @param readAPIKey Read API key associated with the channel.  *If you share code with others, do _not_ share this key*
 * @param ruscb User's callback function to process the server response.
 * @retval false: AsyncTS client is busy. Couldn't send the request.
 * @retval true: request is under sending.
*/
bool AsyncTS::readStringField(unsigned long channelNumber, unsigned int field, const char * readAPIKey, readResponseUserCB ruscb)
{
    if (!_isReady())
    {
        DEBUG_ATS("ats::writeRaw Clinet is busy.");
        _lastTSerrorcode = TS_ERR_CONNECT_FAILED;
        return false;
    }
    _readResponseUserCB = ruscb;
    return readStringField(channelNumber,field, readAPIKey);
}

/**
 * @brief Read the latest string from a public ThingSpeak channel.
 * 
 * @pre First call  onReadServerResponseUserCB(), to set the user's callback function.
 * @post Through user' callback: std::any<string>* need process.
 * @param channelNumber Channel number
 * @param field Field number (1-8) within the channel to read from.
 * @retval false: AsyncTS client is busy. Couldn't send the request.
 * @retval true: request is under sending.
*/
bool AsyncTS::readStringField(unsigned long channelNumber, unsigned int field)
{
    if (!_isReady())
    {
        DEBUG_ATS("ats::writeRaw Clinet is busy.");
        _lastTSerrorcode = TS_ERR_CONNECT_FAILED;
        return false;
    }
    _retValueSelector = [this](){ this->_readStringFieldCB(); };
    return _readStringFieldInternal(channelNumber, field, NULL);
}
/**
 * @brief Read the latest string from a public ThingSpeak channel.
 * @param channelNumber Channel number
 * @param field Field number (1-8) within the channel to read from.
 * @param ruscb User's callback function to process the server response.
 * @post Through user' callback: std::any<string>* need process.
 * @retval false: AsyncTS client is busy. Couldn't send the request.
 * @retval true: request is under sending.
*/
bool AsyncTS::readStringField(unsigned long channelNumber, unsigned int field , readResponseUserCB ruscb)
{
    if (!_isReady())
    {
        DEBUG_ATS("ats::writeRaw Clinet is busy.");
        _lastTSerrorcode = TS_ERR_CONNECT_FAILED;
        return false;
    }
    _readResponseUserCB = ruscb;
    return readStringField(channelNumber,field);   
}

void AsyncTS::_readFloatFieldCB()
{
    if (_readResponseUserCB)
    {
        std::any a = _convertStringToFloat(_response.readString());
        _readResponseUserCB(_lastTSerrorcode, &a);
    }
}
/**
 * @brief Read the latest floating point value from a private ThingSpeak channel
 * @pre First call  onReadServerResponseUserCB(), to set the user's callback function.
 * @post Through user' callback: std::any<float>* points a value or 0 if the field is
 * text or there is an error.
 * @param channelNumber Channel number
 * @param field Field number (1-8) within the channel to read from.
 * @param readAPIKey Read API key associated with the channel.  *If you share code with others, do _not_ share this key*
 * @retval false: AsyncTS client is busy. Couldn't send the request.
 * @retval true: request is under sending.
*/
bool AsyncTS::readFloatField(unsigned long channelNumber, unsigned int field, const char *readAPIKey)
{
    if (!_isReady())
    {
        DEBUG_ATS("ats::writeRaw Clinet is busy.");
        _lastTSerrorcode = TS_ERR_CONNECT_FAILED;
        return false;
    }
    _retValueSelector = [this]()
    { this->_readFloatFieldCB(); };
    return _readStringFieldInternal(channelNumber, field, readAPIKey);
}
/**
 * @brief Read the latest floating point value from a private ThingSpeak channel
 * @param channelNumber Channel number
 * @param field Field number (1-8) within the channel to read from.
 * @param readAPIKey Read API key associated with the channel.  *If you share code with others, do _not_ share this key*
 * @param ruscb User's callback function to process the server response.
 * @post Through user' callback: std::any<float>* points a value or 0 if the field is
 * text or there is an error.
 * @retval false: AsyncTS client is busy. Couldn't send the request.
 * @retval true: request is under sending.
*/
bool AsyncTS::readFloatField(unsigned long channelNumber, unsigned int field, const char * readAPIKey, readResponseUserCB ruscb)
{
    if (!_isReady())
    {
        DEBUG_ATS("ats::writeRaw Clinet is busy.");
        _lastTSerrorcode = TS_ERR_CONNECT_FAILED;
        return false;
    }
    _readResponseUserCB = ruscb;
    return readFloatField(channelNumber , field, readAPIKey);
}
/**
 * @brief Read the latest floating point value from a public ThingSpeak channel
 * @pre First call  onReadServerResponseUserCB(), to set the user's callback function.
 * @post Through user' callback: std::any<float>* points a value or 0 if the field is
 * text or there is an error.
 * @param channelNumber Channel number
 * @param field Field number (1-8) within the channel to read from.
 * @retval false: AsyncTS client is busy. Couldn't send the request.
 * @retval true: request is under sending.
 * @note  NAN, INFINITY, and -INFINITY are valid results.
*/
bool AsyncTS::readFloatField(unsigned long channelNumber, unsigned int field)
{
    return readFloatField(channelNumber, field, NULL);
}
/**
 * @brief Read the latest floating point value from a public ThingSpeak channel
 * @post  Through ruscb: std::any<float>* points a value or 0 if the field is
 * text or there is an error.
 * @param channelNumber Channel number
 * @param field Field number (1-8) within the channel to read from.
 * @param ruscb User's callback function to process the server response.
 * @retval false: AsyncTS client is busy. Couldn't send the request.
 * @retval true: request is under sending.
 * @note  NAN, INFINITY, and -INFINITY are valid results.
*/
bool AsyncTS::readFloatField(unsigned long channelNumber, unsigned int field, readResponseUserCB ruscb)
{
    if (!_isReady())
    {
        DEBUG_ATS("ats::writeRaw Clinet is busy.");
        _lastTSerrorcode = TS_ERR_CONNECT_FAILED;
        return false;
    }
    _readResponseUserCB = ruscb;
    return readFloatField(channelNumber , field, NULL);
}

void AsyncTS::_readLongFieldCB()
{
    if (_readResponseUserCB)
    {
        std::any a = (long)_response.readString().toInt();
        _readResponseUserCB(_lastTSerrorcode, &a);
    }
}

/**
 * @brief Read the latest long value from a private ThingSpeak channel
 * @pre First call  onReadServerResponseUserCB(), to set the user's callback function.
 * @param channelNumber Channel number
 * @param field Field number (1-8) within the channel to read from.
 * @param readAPIKey Read API key associated with the channel.  *If you share code with others, do _not_ share this key*
 * @retval false: AsyncTS client is busy. Couldn't send the request.
 * @retval true: request is under sending.
 * @post Through user's callback function: std::any<long>* points a value or 0 if the field is text or there is an error.
 * @note  NAN, INFINITY, and -INFINITY are valid results.
*/
bool AsyncTS::readLongField(unsigned long channelNumber, unsigned int field, const char *readAPIKey)
{
    if (!_isReady())
    {
        DEBUG_ATS("ats::writeRaw Clinet is busy.");
        _lastTSerrorcode = TS_ERR_CONNECT_FAILED;
        return false;
    }
    _retValueSelector = [this]()
    { this->_readLongFieldCB(); };
    return _readStringFieldInternal(channelNumber, field, readAPIKey);
}

/**
 * @brief Read the latest long value from a private ThingSpeak channel
 * @param channelNumber Channel number
 * @param field Field number (1-8) within the channel to read from.
 * @param readAPIKey Read API key associated with the channel.  *If you share code with others, do _not_ share this key*
 * @param ruscb User's callback function to process the server response.
 * @retval false: AsyncTS client is busy. Couldn't send the request.
 * @retval true: request is under sending.
 * @post  Through ruscb: std::any<long>* points a value or 0 if the field is text or there is an error.
 * @note  NAN, INFINITY, and -INFINITY are valid results.
*/
bool AsyncTS::readLongField(unsigned long channelNumber, unsigned int  field, const char * readAPIKey, readResponseUserCB ruscb)
{
 if (!_isReady())
    {
        DEBUG_ATS("ats::writeRaw Clinet is busy.");
        _lastTSerrorcode = TS_ERR_CONNECT_FAILED;
        return false;
    }
 _readResponseUserCB = ruscb;
 return readLongField(channelNumber, field, readAPIKey);   
}

/**
 * @brief Read the latest long value from a public ThingSpeak channel
 * @pre First call  onReadServerResponseUserCB(), to set the user's callback function.
 * @param channelNumber Channel number
 * @param field Field number (1-8) within the channel to read from.
 * @retval false: AsyncTS client is busy. Couldn't send the request.
 * @retval true: request is under sending.
 * @post  Through user's callback function: std::any<long>* points a value or 0 if the field is text or there is an error.
 * @note  NAN, INFINITY, and -INFINITY are valid results.
*/
bool AsyncTS::readLongField(unsigned long channelNumber, unsigned int field)
{
    return readLongField(channelNumber, field, NULL);
}

/**
 * @brief Read the latest long value from a public ThingSpeak channel
 * @param channelNumber Channel number
 * @param field Field number (1-8) within the channel to read from.
 * @param ruscb User's callback function to process the server response.
 * @retval false: AsyncTS client is busy. Couldn't send the request.
 * @retval true: request is under sending.
 * @post Through ruscb: std::any<long>* points a value or 0 if the field is text or there is an error.
 * @note  NAN, INFINITY, and -INFINITY are valid results.
*/
bool AsyncTS::readLongField(unsigned long channelNumber, unsigned int field, readResponseUserCB ruscb)
{
    if (!_isReady())
    {
        DEBUG_ATS("ats::writeRaw Clinet is busy.");
        _lastTSerrorcode = TS_ERR_CONNECT_FAILED;
        return false;
    }
    _readResponseUserCB = ruscb;
    return readLongField(channelNumber,field, NULL);
}

void AsyncTS::_readIntFieldCB()
{
    if (_readResponseUserCB)
    {
        std::any a = (int)_response.readString().toInt();
        _readResponseUserCB(_lastTSerrorcode, &a);
    }
}

/**
 * @brief Read the latest int value from a private ThingSpeak channel.
 * @pre First call  onReadServerResponseUserCB(), to set the user's callback function.
 * @param channelNumber Channel number
 * @param field   Field number (1-8) within the channel to read from.
 * @param readAPIKey  Read API key associated with the channel.  *If you share code with others, do _not_ share this key*
 * @retval false: AsyncTS client is busy. Couldn't send the request.
 * @retval true: request is under sending.
 * @post  Through user's callback function: std::any<int>* points a value or 0 if the field is text or there is an error.
 * @note  NAN, INFINITY, and -INFINITY are valid results.
*/
bool AsyncTS::readIntField(unsigned long channelNumber, unsigned int field, const char *readAPIKey)
{
    if (!_isReady())
    {
        DEBUG_ATS("ats::writeRaw Clinet is busy.");
        _lastTSerrorcode = TS_ERR_CONNECT_FAILED;
        return false;
    }
    _retValueSelector = [this](){ this->_readIntFieldCB(); };
    return _readStringFieldInternal(channelNumber, field, readAPIKey);
}

/**
 * @brief Read the latest int value from a private ThingSpeak channel.
 * @param channelNumber Channel number
 * @param field  Field number (1-8) within the channel to read from.
 * @param readAPIKey   Read API key associated with the channel.  *If you share code with others, do _not_ share this key*
 * @param ruscb User's callback function to process the server response.
 * @retval false: AsyncTS client is busy. Couldn't send the request.
 * @retval true: request is under sending.
 * @post Through ruscb: std::any<int>* points a value or 0 if the field is text or there is an error.
 * @note  NAN, INFINITY, and -INFINITY are valid results.
*/
bool AsyncTS::readIntField(unsigned long channelNumber, unsigned int field, const char * readAPIKey, readResponseUserCB ruscb)
{
    if (!_isReady())
    {
        DEBUG_ATS("ats::writeRaw Clinet is busy.");
        _lastTSerrorcode = TS_ERR_CONNECT_FAILED;
        return false;
    }
    _readResponseUserCB = ruscb;
    return readIntField(channelNumber,field, readAPIKey);   
}

/**
 * @brief Read the latest int value from a public ThingSpeak channel.
 * @pre First call  onReadServerResponseUserCB(), to set the user's callback function.
 * @param channelNumber Channel number
 * @param field  Field number (1-8) within the channel to read from.
 * @retval false: AsyncTS client is busy. Couldn't send the request.
 * @retval true: request is under sending.
 * @post Through  user's callback function: std::any<int>* points a value or 0 if the field is text or there is an error.
 * @note  NAN, INFINITY, and -INFINITY are valid results.
*/
bool AsyncTS::readIntField(unsigned long channelNumber, unsigned int field)
{
    return readIntField(channelNumber, field, NULL);
}

/**
 * @brief Read the latest int value from a public ThingSpeak channel.
 * @param channelNumber  Channel number
 * @param field  Field number (1-8) within the channel to read from.
 * @param ruscb User's callback function to process the server response.
 * @retval false: AsyncTS client is busy. Couldn't send the request.
 * @retval true: request is under sending.
 * @post Through ruscb: std::any<int>* points a value or 0 if the field is text or there is an error.
 * @note  NAN, INFINITY, and -INFINITY are valid results.
*/
bool AsyncTS::readIntField(unsigned long channelNumber, unsigned int field, readResponseUserCB ruscb)
{
    if (!_isReady())
    {
        DEBUG_ATS("ats::writeRaw Clinet is busy.");
        _lastTSerrorcode = TS_ERR_CONNECT_FAILED;
        return false;
    }
    _readResponseUserCB = ruscb;
    return readIntField(channelNumber, field, NULL);
}

void AsyncTS::_readMultipleFieldsCB()
{
    if (_readResponseUserCB)
    {
        String multiContent = _response.readString();

        this->lastFeed.nextReadField[0] = _parseValues(multiContent, "field1");
        this->lastFeed.nextReadField[1] = _parseValues(multiContent, "field2");
        this->lastFeed.nextReadField[2] = _parseValues(multiContent, "field3");
        this->lastFeed.nextReadField[3] = _parseValues(multiContent, "field4");
        this->lastFeed.nextReadField[4] = _parseValues(multiContent, "field5");
        this->lastFeed.nextReadField[5] = _parseValues(multiContent, "field6");
        this->lastFeed.nextReadField[6] = _parseValues(multiContent, "field7");
        this->lastFeed.nextReadField[7] = _parseValues(multiContent, "field8");
        this->lastFeed.nextReadCreatedAt = _parseValues(multiContent, "created_at");
        this->lastFeed.nextReadLatitude = _parseValues(multiContent, "latitude");
        this->lastFeed.nextReadLongitude = _parseValues(multiContent, "longitude");
        this->lastFeed.nextReadElevation = _parseValues(multiContent, "elevation");
        this->lastFeed.nextReadStatus = _parseValues(multiContent, "status");
        std::any a = this;
        _readResponseUserCB(_lastTSerrorcode, &a);
    }
}

/**
 * @brief Read all the field values, status message, location coordinates, and created-at
 * timestamp associated with the latest feed to a private ThingSpeak channel and store the
 * values locally in variables within a struct.
 * @pre First call  onReadServerResponseUserCB(), to set the user's callback function.
 * @param channelNumber Channel number
 * @param readAPIKey Read API key associated with the channel. *If you share code with others, do _not_ share this key*
 * @retval false: AsyncTS client is busy. Couldn't send the request.
 * @retval true: request is under sending.
 * @post Through  user's callback function you get 200 status code + std::any<AsyncTS>* if successful.
*/
bool AsyncTS::readMultipleFields(unsigned long channelNumber, const char *readAPIKey)
{
    if (!_isReady())
    {
        DEBUG_ATS("ats::writeRaw Clinet is busy.");
        _lastTSerrorcode = TS_ERR_CONNECT_FAILED;
        return false;
    }
    String readCondition = "/feeds/last.txt?status=true&location=true";
    _retValueSelector = [this]()
    { this->_readMultipleFieldsCB(); };
    return _readRaw(channelNumber, readCondition, readAPIKey);
}

/**
 * @brief Read all the field values, status message, location coordinates, and created-at
 * timestamp associated with the latest feed to a private ThingSpeak channel and store the
 * values locally in variables within a struct.
 * @param channelNumber Channel number
 * @param readAPIKey Read API key associated with the channel. *If you share code with others, do _not_ share this key*
 * @param ruscb User's callback function to process the server response.
 * @retval false: AsyncTS client is busy. Couldn't send the request.
 * @retval true: request is under sending.
 * @post Through  user's callback function you get 200 status code + std::any<AsyncTS>* if successful.
*/
bool AsyncTS::readMultipleFields(unsigned long channelNumber, const char * readAPIKey, readResponseUserCB ruscb)
{
    if (!_isReady())
    {
        DEBUG_ATS("ats::writeRaw Clinet is busy.");
        _lastTSerrorcode = TS_ERR_CONNECT_FAILED;
        return false;
    }
    _readResponseUserCB = ruscb;
    return readMultipleFields(channelNumber, readAPIKey);
}

/**
 * @brief Read all the field values, status message, location coordinates, and created-at
 * timestamp associated with the latest feed to a public ThingSpeak channel and store the
 * values locally in variables within a struct.
 * @pre First call  onReadServerResponseUserCB(), to set the user's callback function.
 * @param channelNumber Channel number
 * @retval false: AsyncTS client is busy. Couldn't send the request.
 * @retval true: request is under sending.
 * @post Through  user's callback function you get 200 status code + std::any<AsyncTS>* if successful.
*/
bool AsyncTS::readMultipleFields(unsigned long channelNumber)
{
    return readMultipleFields(channelNumber, NULL);
}

/**
 * @brief Read all the field values, status message, location coordinates, and created-at
 * timestamp associated with the latest feed to a public ThingSpeak channel and store the
 * values locally in variables within a struct.
 * @param channelNumber Channel number
 * @param ruscb User's callback function to process the server response.
 * @retval false: AsyncTS client is busy. Couldn't send the request.
 * @retval true: request is under sending.
 * @post Through  user's callback function you get 200 status code + std::any<AsyncTS>* if successful.
*/
bool AsyncTS::readMultipleFields(unsigned long channelNumber, readResponseUserCB ruscb)
{
    if (!_isReady())
    {
        DEBUG_ATS("ats::writeRaw Clinet is busy.");
        _lastTSerrorcode = TS_ERR_CONNECT_FAILED;
        return false;
    }
    _readResponseUserCB = ruscb;
    return  readMultipleFields(channelNumber, NULL);
}

void AsyncTS::_readStatusCB()
{
    if (_readResponseUserCB)
    {
        std::any a = _getJSONValueByKey(_response.readString(), "status");
        _readResponseUserCB(_lastTSerrorcode, &a);
    }
}

/**
 * @brief Read the latest status from a private ThingSpeak channel.
 * @pre Call onReadServerResponseUserCB() before.
 * @post User's callback need process std::any<String>*.
 * @param channelNumber Channel number
 * @param readAPIKey Read API key associated with the channel.  *If you share code with others, do _not_ share this key*
 * @retval false: AsyncTS client is busy. Couldn't send the request.
 * @retval true: request is under sending.
*/
bool AsyncTS::readStatus(unsigned long channelNumber, const char *readAPIKey)
{
    if (!_isReady())
    {
        DEBUG_ATS("ats::writeRaw Clinet is busy.");
        _lastTSerrorcode = TS_ERR_CONNECT_FAILED;
        return false;
    }
    String content = "/feeds/last.txt?status=true&location=true";
    _retValueSelector = [this]()
    { this->_readStatusCB(); };
    return _readRaw(channelNumber, content, readAPIKey);
}

/**
 * @brief Read the latest status from a private ThingSpeak channel.
 * @post User's callback need process std::any<String>*.
 * @param channelNumber Channel number
 * @param readAPIKey Read API key associated with the channel.  *If you share code with others, do _not_ share this key*
 * @param ruscb User's callback function to process the server response.
 * @retval false: AsyncTS client is busy. Couldn't send the request.
 * @retval true: request is under sending.
*/
bool AsyncTS::readStatus(unsigned long channelNumber, const char * readAPIKey, readResponseUserCB ruscb)
{
    if (!_isReady())
    {
        DEBUG_ATS("ats::writeRaw Clinet is busy.");
        _lastTSerrorcode = TS_ERR_CONNECT_FAILED;
        return false;
    }
    _readResponseUserCB = ruscb;
    return readStatus(channelNumber,readAPIKey);    
}

/**
 * @brief Read the latest status from a public ThingSpeak channel.
 * @pre Call onReadServerResponseUserCB() before.
 * @post User's callback need process std::any<String>*.
 * @param channelNumber Channel number
 * @retval false: AsyncTS client is busy. Couldn't send the request.
 * @retval true: request is under sending.
*/
bool AsyncTS::readStatus(unsigned long channelNumber)
{
    return readStatus(channelNumber, NULL);
}
/**
 * @brief Read the latest status from a public ThingSpeak channel.
 * @post User's callback need process std::any<String>*.
 * @param channelNumber Channel number
 * @param ruscb User's callback function to process the server response.
 * @retval false: AsyncTS client is busy. Couldn't send the request.
 * @retval true: request is under sending.
*/
bool AsyncTS::readStatus(unsigned long channelNumber, readResponseUserCB ruscb)
{
    if (!_isReady())
    {
        DEBUG_ATS("ats::writeRaw Clinet is busy.");
        _lastTSerrorcode = TS_ERR_CONNECT_FAILED;
        return false;
    }
    _readResponseUserCB = ruscb;
    return readStatus(channelNumber,NULL);
}

/**
 * @brief Set the value of a single field that will be part of a multi-field update.
 * @param field  Field number (1-8) within the channel to set.
 * @param value  Integer value (from -32,768 to 32,767) to set.
 * @retval 200 if successful
 * @retval -101 if value is out of range or string is too long (> 255 bytes)
*/
int AsyncTS::setField(unsigned int field, int value)
{
    char valueString[10]; // int range is -32768 to 32768, so 7 bytes including terminator
    itoa(value, valueString, 10);
    return setField(field, valueString);
}

/**
 * @brief Set the value of a single field that will be part of a multi-field update.
 * @param field  Field number (1-8) within the channel to set.
 * @param value  Long value (from -2,147,483,648 to 2,147,483,647) to write.
 * @retval  200 if successful
 * @retval -101 if value is out of range or string is too long (> 255 bytes)
*/
int AsyncTS::setField(unsigned int field, long value)
{
    char valueString[15]; // long range is -2147483648 to 2147483647, so 12 bytes including terminator
    ltoa(value, valueString, 10);

    return setField(field, valueString);
}

/**
 * @brief Set the value of a single field that will be part of a multi-field update.
 * @param field  Field number (1-8) within the channel to set.
 * @param value  Floating point value (from -999999000000 to 999999000000) to write.  If you need more accuracy, or a wider range, you should format the number yourself (using <tt>dtostrf</tt>) and setField() using the resulting string.
 * @retval 200 if successful
 * @retval -101 if value is out of range or string is too long (> 255 bytes)
*/
int AsyncTS::setField(unsigned int field, float value)
{
    char valueString[20]; // range is -999999000000.00000 to 999999000000.00000, so 19 + 1 for the terminator
    int status = _convertFloatToChar(value, valueString);
    if (status != TS_OK_SUCCESS)
        return status;

    return setField(field, valueString);
}

/**
 * @brief Set the value of a single field that will be part of a multi-field update.
 * @param field  Field number (1-8) within the channel to set.
 * @param value  String to write (UTF8).  ThingSpeak limits this to 255 bytes.
 * @retval 200 if successful
 * @retval -101 if value is out of range or string is too long (> 255 bytes)
*/
int AsyncTS::setField(unsigned int field, String value)
{
    DEBUG_ATS("ts::setField   (field: %u value: \"%s\")\r\n", field, value);
    if (field < FIELDNUM_MIN || field > FIELDNUM_MAX)
        return TS_ERR_INVALID_FIELD_NUM;
    // Max # bytes for ThingSpeak field is 255 (UTF-8)
    if (value.length() > FIELDLENGTH_MAX)
        return TS_ERR_OUT_OF_RANGE;
    _nextWriteField[field - 1] = value;

    return TS_OK_SUCCESS;
}

/**
 * @brief Set the latitude of a multi-field update.
 * @note To record latitude, longitude and elevation of a write, call setField() for each of the fields you want to write.
 * Then setLatitude(), setLongitude(), setElevation() and then call writeFields()
 * @param latitude Latitude of the measurement as a floating point value (degrees N, use negative values for degrees S)
 * @return Always return 200
*/
int AsyncTS::setLatitude(float latitude)
{
    DEBUG_ATS("ts::setLatitude(latitude: %f)\r\n", latitude);
    _nextWriteLatitude = latitude;
    return TS_OK_SUCCESS;
}

/**
 * @brief Set the longitude of a multi-field update.
 * @note  To record latitude, longitude and elevation of a write, call setField() for each of the fields you want to write.
 * Then setLatitude(), setLongitude(), setElevation() and then call writeFields()
 * @param longitude  Longitude of the measurement as a floating point value (degrees E, use negative values for degrees W)
 * @return Always return 200
*/
int AsyncTS::setLongitude(float longitude)
{
    DEBUG_ATS("ts::setLongitude(longitude: %f)\r\n", longitude);

    _nextWriteLongitude = longitude;

    return TS_OK_SUCCESS;
}

/**
 * @brief Set the elevation of a multi-field update.
 * @param elevation  Elevation of the measurement as a floating point value (meters above sea level).
 * @return Always return 200.
 * @note To record latitude, longitude and elevation of a write, call setField() for each of the fields you want to write.
 * Then setLatitude(), setLongitude(), setElevation() and then call writeFields().
*/
int AsyncTS::setElevation(float elevation)
{
    DEBUG_ATS("ts::setElevation(elevation: %f)\r\n", elevation);
    _nextWriteElevation = elevation;

    return TS_OK_SUCCESS;
}


/**
 * @brief Set the status field of a multi-field update.
 * To record a status message on a write, call setStatus() then call writeFields().
 * @param status String to write (UTF8).  ThingSpeak limits this to 255 bytes.
 * @retval 200 if successful.
 * @retval -101 if string is too long (> 255 bytes)
 * @note To record a status message on a write, call setStatus() then call writeFields(). 
 * Use status to provide additonal details when writing a channel update.
 * Additonally, status can be used by the ThingTweet App to send a message to Twitter.
 * To record a status message on a write, call setStatus() then call writeFields().
*/
int AsyncTS::setStatus(String status)
{
    DEBUG_ATS("ts::setStatus(status: %s)\r\n",status);

    // Max # bytes for ThingSpeak field is 255 (UTF-8)
    if (status.length() > FIELDLENGTH_MAX)
        return TS_ERR_OUT_OF_RANGE;
    _nextWriteStatus = status;

    return TS_OK_SUCCESS;
}

/**
 * @brief Set the Twitter account and message to use for an update to be tweeted.
 * @pre To send a message to twitter call setTwitterTweet() then call writeFields().
 * @param twitter Twitter account name as a String.
 * @param tweet Twitter message as a String (UTF-8) limited to 140 character.
 * @retval 200 if successful.
 * @retval -101 if string is too long (> 255 bytes)
 * @note To send a message to twitter call setTwitterTweet() then call writeFields().
 * Prior to using this feature, a twitter account must be linked to your ThingSpeak account.
 * Do this by logging into ThingSpeak and going to Apps, then ThingTweet and clicking Link Twitter Account.
*/
int AsyncTS::setTwitterTweet(String twitter, String tweet)
{
    // Max # bytes for ThingSpeak field is 255 (UTF-8)
    if ((twitter.length() > FIELDLENGTH_MAX) || (tweet.length() > FIELDLENGTH_MAX))
        return TS_ERR_OUT_OF_RANGE;

    _nextWriteTwitter = twitter;
    _nextWriteTweet = tweet;

    return TS_OK_SUCCESS;
}

/**
 * @brief Set the created-at date of a multi-field update.
 * @param createdAt  Desired timestamp to be included with the channel update as a String. The timestamp string must be in the ISO 8601 format. Example "2017-01-12 13:22:54"
 * @retval 200 if successful.
 * @retval -101 if string is too long (> 255 bytes)
 * @note Timezones can be set using the timezone hour offset parameter.
 * For example, a timestamp for Eastern Standard Time is: "2017-01-12 13:22:54-05".
 * If no timezone hour offset parameter is used, UTC time is assumed.
*/
int AsyncTS::setCreatedAt(String createdAt)
{
    // the ISO 8601 format is too complicated to check for valid timestamps here
    // we'll need to reply on the api to tell us if there is a problem
    // Max # bytes for ThingSpeak field is 255 (UTF-8)
    if (createdAt.length() > FIELDLENGTH_MAX)
        return TS_ERR_OUT_OF_RANGE;
    _nextWriteCreatedAt = createdAt;

    return TS_OK_SUCCESS;
}

/**
 * @brief Fetch the value as string from the latest stored feed record.
 * @param field Field number (1-8) within the channel to read from.
 * @return Value read (UTF8 string), empty string if there is an error, or old value read (UTF8 string) if invoked before readMultipleFields(). 
 * Use getLastTSErrorCode() to get more specific information.
*/
String AsyncTS::getFieldAsString(unsigned int field)
{
    if (field < FIELDNUM_MIN || field > FIELDNUM_MAX)
    {
        this->_lastTSerrorcode = TS_ERR_INVALID_FIELD_NUM;
        return ("");
    }

    _lastTSerrorcode = TS_OK_SUCCESS;
    return this->lastFeed.nextReadField[field - 1];
}

/**
 * @brief Fetch the value as float from the latest stored feed record.
 * @param field Field number (1-8) within the channel to read from.
 * @return Value read, 0 if the field is text or there is an error, or old value read if invoked before readMultipleFields().Use getLastTSErrorCode() to get more specific information.
 * Note that NAN, INFINITY, and -INFINITY are valid results.
*/
float AsyncTS::getFieldAsFloat(unsigned int field)
{
    return _convertStringToFloat(getFieldAsString(field));
}

/**
 * @brief Fetch the value as long from the latest stored feed record.
 * @return Value read, 0 if the field is text or there is an error, or old value read if invoked before readMultipleFields().
*/
long AsyncTS::getFieldAsLong(unsigned int field)
{
    // Note that although the function is called "toInt" it really returns a long.
    return getFieldAsString(field).toInt();
}

/**
 * @brief Fetch the value as int from the latest stored feed record.
 * @param field - Field number (1-8) within the channel to read from.
 * @return Value read, 0 if the field is text or there is an error, or old value read if invoked before readMultipleFields().
 * @note Use getLastTSErrorCode() to get more specific information.
*/
int AsyncTS::getFieldAsInt(unsigned int field)
{
    // int and long are same
    return getFieldAsLong(field);
}

/**
 * @brief Fetch the status message associated with the latest stored feed record.
 * @return Value read (UTF8 string). An empty string is returned if there was no elevation written to the channel or in case of an error.
 * @note Use getLastTSErrorCode() to get more specific information.
*/
String AsyncTS::getStatus()
{
    return this->lastFeed.nextReadStatus;
}

/**
 * @brief Fetch the latitude associated with the latest stored feed record.
 * @return Value read (UTF8 string). An empty string is returned if there was no elevation written to the channel or in case of an error.
 * @note Use getLastTSErrorCode() to get more specific information.
*/
String AsyncTS::getLatitude()
{
    return this->lastFeed.nextReadLatitude;
}

/**
 * @brief Fetch the longitude associated with the latest stored feed record.
 * @return Value read (UTF8 string). An empty string is returned if there was no elevation written to the channel or in case of an error.
 * @note Use getLastTSErrorCode() to get more specific information.
*/
String AsyncTS::getLongitude()
{
    return this->lastFeed.nextReadLongitude;
}

/**
 * @brief Fetch the elevation associated with the latest stored feed record.
 * @return Value read (UTF8 string). An empty string is returned if there was no elevation written to the channel or in case of an error.
 * @note Use getLastTSErrorCode() to get more specific information.
*/
String AsyncTS::getElevation()
{
    return this->lastFeed.nextReadElevation;
}

/**
 * @brief Fetch the created-at timestamp associated with the latest stored feed record.
 * @return Value read (UTF8 string). An empty string is returned if there was no created-at timestamp written to the channel or in case of an error.
 * @note Use getLastTSErrorCode() to get more specific information.
*/
String AsyncTS::getCreatedAt()
{
    return this->lastFeed.nextReadCreatedAt;
}