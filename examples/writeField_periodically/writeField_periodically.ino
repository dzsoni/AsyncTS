
#include <any>
#include <TaskScheduler.h>    //https://github.com/arkhipenko/TaskScheduler
#include <AsyncTS.h>

#ifdef ARDUINO_ARCH_ESP32
#include <WiFi.h>
#include <AsyncTCP.h>
#elif defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#endif

#define MIN_THINGSPEAK_DELAY 15000 // 15sec
void writeThingSpeak();//forward declaration


const char ssid[] = "";                      // Set your WiFi SSID
const char password[] = "";                  // Set your WiFi password
unsigned long channelID = 111111;            // Set your channel ID  
const char writeAPIkey[] = "";               //Set your writeAPIkey
const char readAPIkey[] = "";                //Set your readAPIkey


unsigned long start;
int value=0;
int field=1;
AsyncClient client;
AsyncTS ats;
Scheduler taskManager;
Task writeTS(MIN_THINGSPEAK_DELAY, TASK_FOREVER, &writeThingSpeak, &taskManager, false);

// User callback function for write actions 
void writeServerResponse(int code)
{
  Serial.println("Response time:"+String(millis()-start)+"ms");
  Serial.println("Server response:" + String(code));
  writeTS.disable();
  writeTS.enableDelayed();
}
//send data periodically to ThingSpeak
void writeThingSpeak()
{
 if(!ats.writeField(channelID,field,String(value),writeAPIkey))
  {
    Serial.println("Can't send.");
  }
  else{
    Serial.println("Sent:"+String(value));
    start=millis();
    value++;
  }
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
  Serial.println("\r\nWiFi connected.");

  ats.onWriteServerResponseUserCB(writeServerResponse);
  ats.begin(client);
  writeTS.enable();
}

void loop() {
  taskManager.execute();
}
