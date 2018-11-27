
// BOARD: LOLIN(WEMOS) D1 R2 Mini // BAUDRATE: 115200 // CPU Frecuency: 80 MHz // Programmer: AVRISP mkll

#include <iostream>
#include "btc_pool.hpp"
#include "global.h"
#include "tcp_client.hpp"
#include "OLEDDisplayUi.h"
#include "font16.h" 
#include "SSD1306Wire.h"
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include "ESP8266WiFi.h"
#include "ESP8266Ping.h"
#include <WiFiManager.h>
#include "config.h" 

//------defines for tcp----------------------------
#define SSID_STR "stratum.slushpool.com"
#define PASSWORD "slushpool"
#define LOGIN "gggabriella.puertorico3" //gggabriella.puertorico
#define TCP_PORT 3333

//eliminar si uso WiFiManager
//#define WifiSSID "Orange Juice Mami"
//#define WifiPASSWORD "vitaminc"

#define DNS_PORT 53
#include <ESPAsyncTCP.h>

extern "C" {
#include <osapi.h>
#include <os_type.h>
#include "user_interface.h"
}

AsyncClient* client = new AsyncClient;

char rcvBuff[4096];
int rcv_l;
char trmBuff[1024];
unsigned int feed_nonce;
unsigned int next_nonce;
bool connected_to_pool;
bool wait_server;
bool init_miner;

bool executed = false;
unsigned long start;
unsigned long ended;
unsigned long delta;

unsigned int hash_counter;

// uptime timer
int Hour=0;
int Minute=0;
int Second=0;
int HighMillis=0;
int Rollover=0;
String stringhour,stringmin,stringsec;

#include <Ticker.h>
Ticker tickerSet;
unsigned int cur_nonce;
unsigned int cur_nonce_old;

int read_enable;
int second_counter;
char  check_hash[32];
int switchPin = SW_PIN_NUMBER;

// latency/ping info
const char* remote_host = "www.google.com";
String avg_time_ms;
int i;


// Pins based on your wiring
#define SCL_PIN D4
#define SDA_PIN D3
SSD1306Wire  display(0x3c, SDA_PIN, SCL_PIN);
OLEDDisplayUi ui     ( &display );

int screen_out;
int status_miner;

//-------------global------------------------------
btc::StratumPool pool(SSID_STR, TCP_PORT,  LOGIN, PASSWORD);

btc::BTCBlock block;
//int buf_read_count;
//-------------------------------------------------

void f_trmBuff()
{
  client->add(trmBuff, strlen(trmBuff));
  client->send();
  //Serial.println("Snd trmBuff:");
  //Serial.println(trmBuff);
  //  strcpy(rcvBuff, line.c_str());
  delay(50);
}


int f_alt_recv(char *data)
{
  if (rcv_l > 0) {
    data[0] = rcvBuff[0];
    for (int n = 0 ; n < sizeof(rcvBuff) - 1 ; n++)
      rcvBuff[0 + n] = rcvBuff[1 + n];

    rcv_l = rcv_l - 1;
    return rcv_l;
  }
  else
    return 0;
}


static void replyToServer(void* arg) {
  connected_to_pool = true;
}

/* event callbacks */
static void handleData(void* arg, AsyncClient* client, void *data, size_t len) {
  /*
    Serial.printf("\n data received from %s \n", client->remoteIP().toString().c_str());
    Serial.write((uint8_t*)data, len);
  */
  char buf[len + 1];
  strncpy (buf, (char*)data, sizeof (buf));
  for (int n = rcv_l; n < len + rcv_l; n++)
    rcvBuff[n] = buf[n - rcv_l];
  /*
    // line += (char*)data;
    line.replace(rcv_l, len, (char*)data);*/
  rcv_l += len;
  /* Serial.println("Recv by ESP and stored ");
    // Serial.println(line.c_str());
    Serial.println(rcv_l);*/
}

void onConnect(void* arg, AsyncClient* client) {
  //  Serial.printf("\n client has been connected to %s on port %d \n", SSID_STR, TCP_PORT);
  replyToServer(client);
}

// DISPLAY INTRO
void intro(void) {
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(URW_Chancery_L_Medium_Italic_16);
  display.drawString(64, 0, F("BTC BTC BTC BTC BTC BTC BTC BTC "));
  display.setFont(ArialMT_Plain_16);
  display.drawString(64, 23, F("MINE YOUR"));
  display.setFont(ArialMT_Plain_16);
  display.drawString(64, 43, F("OWN BUSINESS"));
  display.display();   
  delay(4000);
  yield();   
}


//FIXED DISPLAY #1 (NONCE/HASHRATE)
void msOverlay(OLEDDisplay *display, OLEDDisplayUiState* state) {   
  int hashrate = (double)SPEED_CYCLE*1000/delta;
 
  
  if (screen_out == 5) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_24);
  display->drawString(64, 20, String(feed_nonce));
  
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(0, 0, String("HashRate "));
  display->drawString(48, 0, String(((double)100000vit/delta)*((double)1000/SPEED_CYCLE)));

  
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(97, 0, String(stringhour));
  display->drawString(100, 0, String(":"));
  display->drawString(113, 0, String(stringmin));
  display->drawString(116, 0, String(":"));
  display->drawString(128, 0, String(stringsec));

    display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(0, 10, String("HashesComp "));
  display->drawString(64, 10, String(hash_counter));

  }
}

//FIXED DISPLAY #2 (PING/TX)
void msOverlay2(OLEDDisplay *display, OLEDDisplayUiState* state) {
const char* remote_host = "www.google.com";
String avg_time_ms;
int i;

long rssi = WiFi.RSSI(); 

//ping remote host put the device in slow motion, so It only gets executed once
  while (executed==false && WiFi.status() == WL_CONNECTED) {

 if (Ping.ping(remote_host)) 
    {
     Ping.ping(remote_host, 1);  //10 time ping to google, You can change value to higher or lower
     i= Ping.averageTime();
     avg_time_ms = Ping.averageTime(); // reading string and Int for easy display integration.
     Serial.println(i);
     } 

  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(0, 0, String("Rssi "));
  display->drawString(38, 0, String(rssi));
  display->drawString(58, 0, String("dBm"));
    
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(0, 10, String("NetSpeed/Ping "));
  display->drawString(74, 10, String(avg_time_ms));
  display->drawString(87, 10, String("ms"));
  
  delay(9000);
  executed = true;
   yield();
  
  }

}

void drawFrame1(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {

 display->setTextAlignment(TEXT_ALIGN_LEFT);
   display->setFont(ArialMT_Plain_10);
  if (screen_out == 5) {
     display->drawString(x + 6, y + 52, String("Verify block..."));
}
}

void drawFrame2(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextAlignment(TEXT_ALIGN_LEFT);
   display->setFont(ArialMT_Plain_10);
  if (screen_out == 4) {
     display->drawString(x + 80, y + 52, String("TE COGE EL HOLANDES $$$"));
  } if (screen_out == 5) {
    display->drawString(x + 6, y + 52, String("Verify block..."));
     display->drawString(x + 80, y + 52, String("FAILED!"));
  }
 
  } 
  

//DISPLAY INFO
// frames are the single views that slide in and how many frames?
FrameCallback frames[] = { drawFrame1, drawFrame2 };
int frameCount = 2;
// Overlays are statically drawn on top of a frame eg. a clock and how many?
OverlayCallback overlays[] = { msOverlay, msOverlay2 };
int overlaysCount = 2;


void setup() {
  Serial.begin(115200);
  
  status_miner = STATUS_WIFI_CONNECT;
  cur_nonce = 0;
  pinMode(switchPin, INPUT_PULLUP);
  delay(100);
  yield();
  
  connected_to_pool = false;
  wait_server = false;
  init_miner = false;
  // buf_read_count = 0;
  // line.reserve(4096);
  for (int n = 0; n < sizeof(rcvBuff); n++)
    rcvBuff[n] = 0;

// Initialising the display
  display.init();
  ui.init();
  display.flipScreenVertically();

  intro();
  display.clear();

  ui.setTargetFPS(60);
  ui.setFrames(frames, frameCount);  // Add frames
  ui.setOverlays(overlays, overlaysCount);  // Add overlays
  ui.setFrameAnimation(SLIDE_LEFT);

 displayMessage(F("Connect to signal 'AreYouMine' and enable access to WiFi."));

// connects to access point
 WiFiManager wifiManager;
 wifiManager.autoConnect("AreYouMine");


//Eliminar sección con WiFiManager
  //WiFi.mode(WIFI_STA);
  //WiFi.begin(WifiSSID, WifiPASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(500);
    yield();
  }
  
  display.clear();
  Serial.println("Wifi connected");
  status_miner = STATUS_WIFI_CONNECTED;
  
  
  client->onData(&handleData, client);
  client->onConnect(&onConnect, client);
  client->connect(SSID_STR, TCP_PORT);
 
  // btc::StratumPool pool(SSID_STR, TCP_PORT,  LOGIN, PASSWORD);
  // btc::BTCBlock block;
  tickerSet.attach_ms(SPEED_CYCLE, Tick, 1);
}


void Tick(int n){

  switch (status_miner)
  {

    case STATUS_WIFI_CONNECT:
      Serial.println("STATUS_WIFI_CONNECT");
      screen_out = 1;
      break;

    case STATUS_WIFI_CONNECTED:
      Serial.println("STATUS_WIFI_CONNECTED");
      screen_out = 2;
      break;

    case STATUS_POOL_DATA_EXC:
      Serial.println("STATUS_POOL_DATA_EXC");
      display.clear();
      screen_out = 3;
      break;

    case STATUS_MAIN_WORK:
      uptime();
      start = micros();
      Serial.println("STATUS_MAIN_WORK");
      WiFi.disconnect();
      unsigned int nonce = random(4294967295);
      feed_nonce = nonce;
      next_nonce = feed_nonce+1;

      String nonce_str;
      nonce_str = "nonce: ";
      nonce_str = String(nonce);
      Serial.println("nonce:");
      Serial.println(nonce);
   
      if (digitalRead(switchPin)) 
      {
        if (work(nonce)){
          Serial.println("NO ME JODAS!!!");
          screen_out = 4;
         }
        else
          {Serial.println("FAILED");
          screen_out = 5;
          }
          
      }
      ended = micros();
      delta = ended - start;
      hash_counter++;
      Serial.println("Looking for next nonce:");
      Serial.println(nonce+1);    
      
      break;
      
  }

}

  int work(unsigned int nonce) {
  block = pool.getNewBlock();
  return cur_nonce;
  }

  void displayMessage(String message){
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawStringMaxWidth(0, 10, 128, message);
  display.display();
  }

void loop() {

  ESP.wdtFeed();
  yield();

 

  //DISPLAY STUFF
 int remainingTimeBudget = ui.update();

  if (remainingTimeBudget > 0) {
    delay(remainingTimeBudget);
    yield();
  }

  //if (screen_out == 3) {
   // displayMessage(F("Connecting to blockchain"));
  //} else {}

  if (connected_to_pool) {
//ESP.wdtFeed();
    if (!init_miner)
    {
      btc::StratumPool pool(SSID_STR, TCP_PORT,  LOGIN, PASSWORD);
      btc::BTCBlock block;
      init_miner = true;
      
    }


    if (pool.detectNewBlock())
    {
   //   ESP.wdtFeed();
      block = pool.getNewBlock();
      
    }
    if (status_miner !=  STATUS_MAIN_WORK)
    {
   //   ESP.wdtFeed();
      status_miner =  STATUS_POOL_DATA_EXC;
      //solve your block
   //   ESP.wdtFeed();
      pool.submit(block);
   //   ESP.wdtFeed();
      
    }
  }
  else
  {

  }

}

char *convert(const std::string & s)
{
  char *pc = new char[s.size() + 1];
  std::strcpy(pc, s.c_str());
  return pc;
}


namespace std {

void __throw_bad_cast(void) 
{         
  
  }
void __throw_ios_failure(char const*) {}
void __throw_runtime_error(char const*) {}


} 


void uptime(){
  
  //Making Note of an expected rollover   
  if(millis()>=3000000000){ 
  HighMillis=1;
  }
  //Making note of actual rollover 
  if(millis()<=100000&&HighMillis==1){
  Rollover++;
  HighMillis=0;
  }

  long secsUp = millis()/1000;
  Second = secsUp%60;
  Minute = (secsUp/60)%60;
  Hour = (secsUp/(60*60))%24;
  //Day = (Rollover*50)+(secsUp/(60*60*24));  //First portion takes care of a rollover [around 50 days] 

  stringhour = Hour;
  if (Hour < 10) {
    stringhour = 0 + stringhour;
  }
  stringmin = Minute;
  if (Minute < 10) {
    stringmin = 0 + stringmin;
  }
  stringsec = Second;
   if (Second < 10) {
    stringsec = 0 + stringsec;
  }
}
