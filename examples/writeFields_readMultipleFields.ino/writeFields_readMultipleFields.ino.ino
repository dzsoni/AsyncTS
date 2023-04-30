
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

void readReadMultipleFieldsResponse(int code,std::any* resp)
{
  
  Serial.println("Server response:"+String(code));
  if(code!=200) return;
  auto a = std::any_cast<AsyncTS*>(resp);         //AsyncTS ** a;
  if (!a) 
    {
        // Type-mismatch
        Serial.println("Bad_any_cast <AsyncTS*>");
        return;
    }
  Serial.println((*a)->getFieldAsString(1));    //"1" was string
  Serial.println((*a)->getFieldAsInt(2));       //"2" was String
  Serial.println((*a)->getFieldAsFloat(3),5);   //3.12345f was float
  Serial.println((*a)->getFieldAsLong(4));      //444444444L was Long

  Serial.println((*a)->getFieldAsString(5));    //"five" was String
  Serial.println((*a)->getFieldAsString(6));    //"six" was String
  Serial.println((*a)->getFieldAsString(7));    //"seven" was String
  Serial.println((*a)->getFieldAsString(8));    //"eight" was String

  Serial.println((*a)->getStatus());
  Serial.println((*a)->getLatitude());
  Serial.println((*a)->getLongitude());
  Serial.println((*a)->getElevation());

  Serial.println((*a)->getCreatedAt());
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

  ats.setField(1,"1");        //String
  ats.setField(2,"2");        //String
  ats.setField(3,3.12345f);   //float
  ats.setField(4, 444444444L);//Long
  ats.setField(5,"five");     //String
  ats.setField(6,"six");      //String
  ats.setField(7,"seven");    //String
  ats.setField(8,"eight");    //String
  ats.setStatus("OK");
  ats.setLatitude(50.123455F);
  ats.setLongitude(50.123455F);
  ats.setElevation(80.12345F);

  Serial.println("***************************************************");
  Serial.println("          writeFields");
  Serial.println("***************************************************");
  while(!ats.writeFields(channelID,writeAPIkey))
  {
    delay(1000);
  }
    //..........wait..............
   delay(MIN_THINGSPEAK_DELAY);
   
  
  Serial.println("***************************************************");
  Serial.println("          readMultipleFields");
  Serial.println("***************************************************");
  ats.onReadServerResponseUserCB(readReadMultipleFieldsResponse);
  while(!ats.readMultipleFields(channelID,readAPIkey))
  {
    delay(1000);
  }

}

void loop() {
  // put your main code here, to run repeatedly:
}
