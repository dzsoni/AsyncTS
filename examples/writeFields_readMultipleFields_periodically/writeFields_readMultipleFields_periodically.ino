
#include <any>
#include <AsyncTS.h>
#include <TaskScheduler.h>    //https://github.com/arkhipenko/TaskScheduler

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

unsigned long start;
AsyncClient client;
AsyncTS ats;
Scheduler taskManager;

void writeThingSpeak()
{
  Serial.println("***************************************************");
  Serial.println("          writeFields");
  Serial.println("***************************************************");

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

  if(!ats.writeFields(channelID,writeAPIkey))
  {
    Serial.println("Couldn't send write request to ThingSpeak.");
  }
  start=millis();
}

void readThingSpeak()
{
  Serial.println("***************************************************");
  Serial.println("          readMultipleFields");
  Serial.println("***************************************************");
  
  if(!ats.readMultipleFields(channelID,readAPIkey))
  {
    Serial.println("Couldn't send read request to ThingSpeak.");
  }
  start=millis();
}

Task writeTS(MIN_THINGSPEAK_DELAY, TASK_FOREVER, &writeThingSpeak, &taskManager, false);
Task readTS (MIN_THINGSPEAK_DELAY, TASK_FOREVER, &readThingSpeak, &taskManager, false);

// User call back function for Thingspeak write actions 
void writeFieldsServerResponseCB(int code)
{
  Serial.println("Response time:"+String(millis()-start)+"ms");
  Serial.println("Server response:" + String(code));

  writeTS.disable();
  readTS.enableDelayed();
}

void readMultipleFieldsServerResponseCB(int code,std::any* resp)
{
  Serial.println("Response time:"+String(millis()-start)+"ms");
  Serial.println("Server response:"+String(code));
  if(code!=200) return;
  auto a = std::any_cast<AsyncTS*>(resp);         //AsyncTS* a;
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
  readTS.disable();
  writeTS.enableDelayed();
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

  ats.onWriteServerResponseUserCB(writeFieldsServerResponseCB);
  ats.onReadServerResponseUserCB(readMultipleFieldsServerResponseCB);
  ats.begin(client);

  writeTS.enable();
}

void loop() {
  taskManager.execute();
}
