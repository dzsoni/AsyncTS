
#include <any>
#include <AsyncTS.h>

#ifdef ARDUINO_ARCH_ESP32
#include <WiFi.h>
#include <AsyncTCP.h>
#elif defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#endif

#define MIN_THINGSPEAK_DELAY 15000 // 15sec

const char ssid[] = "";                      // Set your WiFi SSID
const char password[] = "";                  // Set your WiFi password
unsigned long channelID = 111111;            // Set your channel ID  
const char writeAPIkey[] = "";               // Set your writeAPIkey
const char readAPIkey[] = "";                // Set your readAPIkey


AsyncClient client;
AsyncTS ats;

// User call back function for write actions 
void writeServerResponse(int code)
{
  Serial.println("Server response:" + String(code));
}
// User call back  function for read funtions (expect String)
void readServerIntResponse(int code, std::any* resp)
{
    auto* a = std::any_cast<int>(resp);
    if (!a) 
    {
        // Type-mismatch
        Serial.println(" Bad_any_cast <Int>");
        return;
    }
    
         Serial.println("Response code:" + String(code));
         Serial.println("Response: " + String(*a));
}


void setup() {
  const char *ssidpt = ssid;
  const char *passwpt = password;
  
  Serial.begin(115200);
  Serial.println();
  Serial.println("Booted.");
  Serial.println("Connecting to Wi-Fi");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssidpt, passwpt);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.println("WiFi connected.");

  ats.onWriteServerResponseUserCB(writeServerResponse);
  ats.begin(client);

  Serial.println("***************************************************");
  Serial.println("          writeField");
  Serial.println("***************************************************");
  Serial.println("Sending datat to ThingSpeak.");
  while(!ats.writeField(channelID,1,12345,writeAPIkey))
  {
    Serial.println("Can't send.");
    delay(1000);
  }

  delay(MIN_THINGSPEAK_DELAY);

  Serial.println("***************************************************");
  Serial.println("          readIntField");
  Serial.println("***************************************************");

  ats.onReadServerResponseUserCB(readServerIntResponse);
  while(!ats.readIntField(channelID,1,readAPIkey))
  {
   delay(1000);
  }

}

void loop() {
  // put your main code here, to run repeatedly:
}
