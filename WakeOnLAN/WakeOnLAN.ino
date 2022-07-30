/*****************************************************************************/
/*****************************************************************************/
/*               STATUS: WORKING                                             */
/*            TESTED IN: Node MCU ESP12E                                     */
/*                   AT: 2022.07.30                                          */
/*     LAST COMPILED IN: Liliana                                             */
/*****************************************************************************/
/*****************************************************************************/

#define clid "wol_liliana"




#include <ESP8266HTTPClient.h> // Uses core ESP8266WiFi.h internally
#include <ArduinoJson.h>
#include <ArduinoJson.hpp>
#include "WakeOnLan.h"



// *************************************** CONFIG "config_wifi_roy"

#include <config_wifi_roy.h>

#define EEPROM_ADDR_CONNECTED_SSID 1       // Start saving connected network SSID from this memory address
#define EEPROM_ADDR_CONNECTED_PASSWORD 30  // Start saving connected network Password from this memory address
#define AP_SSID clid                       // Set your own Network Name (SSID)
#define AP_PASSWORD "12345678"             // Set your own password

ESP8266WebServer server(80);
// WiFiServer wifiserver(80);

// ***************************************



// *************************************** CONFIG "no_poll_subscriber"

#include <NoPollSubscriber.h>

#define SUB_HOST "notify.estudiosustenta.myds.me"
#define SUB_PORT 80
// #define SUB_HOST "192.168.1.72"
// #define SUB_PORT 1010
#define SUB_PATH "/"

// Declaramos o instanciamos un cliente que se conectará al Host
WiFiClient sub_WiFiclient;

// ***************************************



// *************************************** CONFIG Wake On Lan

WiFiUDP UDP;
/**
 * The Magic Packet needs to be sent as BROADCAST in the LAN
 */
IPAddress computer_ip(255,255,255,255); 

/**
 * The targets MAC address to send the packet to
 */
// byte mac_0[] = { 0x18, 0xC0, 0x4D, 0x6C, 0xD0, 0xE6 };

void sendWOL();

// ***************************************



// *************************************** CONFIG THIS SKETCH

#define PIN_LED_OUTPUT 5 // Pin de LED que va a ser controlado // before was LED_BUILTIN instead of 5
#define PIN_LED_CTRL 15 // Pin que cambia/controla el estado del LED en PIN_LED_OUTPUT manualmente (TOGGLE LED si cambia a HIGH), si se presiona mas de 2 degundos TOGGLE el modo AP y STA+AP

byte PIN_LED_CTRL_VALUE;

#define PUB_HOST "estudiosustenta.myds.me"
// #define PUB_HOST "192.168.1.72"

/***** SET PIN_LED_CTRL TIMER TO PRESS *****/
unsigned long lc_timestamp = millis();
unsigned int  lc_track = 0;
unsigned int  lc_times = 0;
unsigned int  lc_timeout = 2 * 1000; // 2 seconds

// ***************************************





///////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////// CONNECTION ///////////////////////////////////////////

HTTPClient http;
WiFiClient wifiClient;

/////////////////////////////////////////////////
/////////////////// HTTP GET ///////////////////

// httpGet("http://192.168.1.80:69/");
String httpGet(String url) {

  String response;

  http.begin(wifiClient, url.c_str());
  
  // Send HTTP GET request
  http.addHeader("X-Auth-Bearer", clid);
  int httpResponseCode = http.GET();
  
  Serial.print("HTTP Response code: ");
  Serial.println(httpResponseCode);
  
  if ( httpResponseCode > 0 ) {
    if ( httpResponseCode >= 200 && httpResponseCode < 300 ) {
      response = http.getString();
    } else {
      response = "";
    }

  } else {
    response = "";
    Serial.print("[HTTP] GET... failed, error: ");
    Serial.println(http.errorToString(httpResponseCode).c_str());
  }

  // Free resources
  http.end();

  return response;
}

/////////////////////////////////////////////////
/////////////////// HTTP POST ///////////////////

// httpPost("http://192.168.1.80:69/", "application/json", "{\"hola\":\"mundo\"}");
String httpPost(String url, String contentType, String data) {

  String response;

  http.begin(wifiClient, url.c_str());
  
  // Send HTTP GET request
  http.addHeader("Content-Type", contentType);
  http.addHeader("X-Auth-Bearer", clid);
  int httpResponseCode = http.POST(data);
  
  if (httpResponseCode>0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    response = http.getString();
  } else {
    Serial.print("HTTP Request error: ");
    Serial.println(httpResponseCode);
    Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpResponseCode).c_str());
    response = String("HTTP Response code: ") + httpResponseCode;
  }
  // Free resources
  http.end();

  return response;
}





///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////// HELPERS /////////////////////////////////////////////

/////////////////////////////////////////////////
//////////////// KEEP-ALIVE LOOP ////////////////

int connId;
String connSecret;
long connTimeout;
bool runAliveLoop = false;

/***** SET TIMEOUT *****/
unsigned long al_timestamp = millis();
/*unsigned int  al_times = 0;*/
unsigned long  al_timeout = 0;
unsigned int  al_track = 0; // al_track = al_timeout; TO: execute function and then start counting

void handleAliveLoop() {

  if (!runAliveLoop) {
    return;
  }

  al_timeout = connTimeout - 3000; // connTimeout - 3 seconds

  if ( millis() != al_timestamp ) { // "!=" intead of ">" tries to void possible bug when millis goes back to 0
    al_track++;
    al_timestamp = millis();
  }

  if ( al_track > al_timeout ) {
    // DO TIMEOUT!
    /*al_times++;*/
    al_track = 0;

    // RUN THIS FUNCTION!
    String response = httpPost(String("http://") + SUB_HOST + ":" + SUB_PORT + "/alive", "application/json", String("{\"connid\":") + connId + ",\"secret\":\"" + connSecret + "\"}");

    ///// Deserialize JSON. from: https://arduinojson.org/v6/assistant

    // Stream& response;

    StaticJsonDocument<96> doc;

    DeserializationError error = deserializeJson(doc, response);

    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      runAliveLoop = false;
      return;
    }

    int connid = doc["connid"]; // 757
    const char* secret = doc["secret"]; // "zbUIE"
    long timeout = doc["timeout"]; // 300000

    ///// End Deserialize JSON

    connId = connid;
    connSecret = (String)secret;
    connTimeout = timeout;
    runAliveLoop = true;
  }

}

/////////////////////////////////////////////////
//////////////// DETECT CHANGES ////////////////

bool changed(byte gotStat, byte &compareVar) {
  if(gotStat != compareVar){
    /*
    Serial.print("CHANGE!. Before: ");
    Serial.print(compareVar);
    Serial.print(" | After: ");
    Serial.println(gotStat);
    */
    compareVar = gotStat;
    return true;
  } else {
    /*
    Serial.print("DIDN'T CHANGE... Before: ");
    Serial.print(compareVar);
    Serial.print(" | After: ");
    Serial.println(gotStat);
    */
    //compareVar = gotStat;
    return false;
  }
}

/////////////////////////////////////////////////
/////////////// FUNCTIONS IN LOOP ///////////////

void doInLoop() {

  //Serial.println("loop");

  handleAliveLoop();
  
  wifiConfigLoop(server);

  if ( digitalRead(PIN_LED_CTRL) == 1 ) { // IF PIN_LED_CTRL IS PRESSED
    
    // START TIMER
    if ( millis() != lc_timestamp ) { // "!=" intead of ">" tries to void possible bug when millis goes back to 0
      lc_track++;
      lc_timestamp = millis();
    }

    if ( lc_track > lc_timeout ) {
      // DO TIMEOUT!
      lc_times++;
      lc_track = 0;

      if (lc_times == 1) {
        // TOGGLE SERVER STATUS
        Serial.println("Toggling Access Point...");
        //Serial.println(WiFi.getMode());
        switch (WiFi.getMode()) {
          case WIFI_OFF: // case 0:
            //Serial.println("WiFi off");
            break;
          case WIFI_STA: // case 1:
            //Serial.println("Station (STA)");
            ESP_AP_STA(server, AP_SSID, AP_PASSWORD);
            break;
          case WIFI_AP: // case 2:
            //Serial.println("Access Point (AP)");
            break;
          case WIFI_AP_STA: // case 3:
            //Serial.println("Station and Access Point (STA+AP)");
            ESP_STATION(server, true);
            break;
          default:
            Serial.print("WiFi mode NPI xD: ");
            Serial.println(WiFi.getMode());
            break;
        }
      }
    }
  }
  
  bool ledCtrlChanged = changed(digitalRead(PIN_LED_CTRL), PIN_LED_CTRL_VALUE);

  if (ledCtrlChanged) {
    
    if (PIN_LED_CTRL_VALUE == 0) { // IF PIN_LED_CTRL WAS RELEASED

      bool do_toggle = false; // this way is for resetting the state as fast as possible, to not wet stuck waiting for HTTP requests...
      if ( lc_times < 1 ) {
        do_toggle = true;
      }
      
      lc_times = 0; // RESET COUNTER
      lc_track = 0; // RESET TIMER

      if (do_toggle) { // IF PIN_LED_CTRL WAS PRESSED LESS THAN 2 SECONDS...
      }
    }
  }
  

}

/////////////////////////////////////////////////
///////////// DO ON PARSED PAYLOAD /////////////

void onParsed(String line) {

  Serial.print("Got JSON: ");
  Serial.println(line);



  ///// Deserialize JSON. from: https://arduinojson.org/v6/assistant

  // Stream& line;

  StaticJsonDocument<512> doc;

  DeserializationError error = deserializeJson(doc, line);

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }

  long long iat = doc["iat"]; // 1659215080499

  const char* ep_requested = doc["ep"]["requested"]; // "controll/wol_controller/wol_liliana/req"
  const char* ep_emitted = doc["ep"]["emitted"]; // "controll/wol_controller/wol_liliana/req"

  const char* e_type = doc["e"]["type"]; // "info"

  JsonObject e_detail = doc["e"]["detail"];
  const char* e_detail_device = e_detail["device"]; // "pc_liliana"
  int e_detail_whisper = e_detail["whisper"]; // 1284
  const char* e_detail_data = e_detail["data"]; // "1C:1B:0D:9E:E0:49"
  int e_detail_connid = e_detail["connid"]; // 1283
  const char* e_detail_secret = e_detail["secret"]; // "M2mzV"
  long e_detail_timeout = e_detail["timeout"]; // 300000

  ///// End Deserialize JSON



  if (strcmp(e_type, "wol") == 0) {
    Serial.print("Servidor pide despertar a ");
    Serial.println(e_detail_device);

    // // Parse IP
    // byte myIP[4];
    // sscanf(val.c_str(), "%hhu.%hhu.%hhu.%hhu", &myIP[0], &myIP[1], &myIP[2], &myIP[3]);

    // Parse MAC address
    byte macAddrTarget[6];
    sscanf(e_detail_data, "%2hhx:%2hhx:%2hhx:%2hhx:%2hhx:%2hhx", &macAddrTarget[0], &macAddrTarget[1], &macAddrTarget[2], &macAddrTarget[3], &macAddrTarget[4], &macAddrTarget[5]);

    Serial.print(macAddrTarget[0], HEX);
    Serial.print(macAddrTarget[1], HEX);
    Serial.print(macAddrTarget[2], HEX);
    Serial.print(macAddrTarget[3], HEX);
    Serial.print(macAddrTarget[4], HEX);
    Serial.println(macAddrTarget[5], HEX);

    Serial.print(macAddrTarget[0], DEC);
    Serial.print(macAddrTarget[1], DEC);
    Serial.print(macAddrTarget[2], DEC);
    Serial.print(macAddrTarget[3], DEC);
    Serial.print(macAddrTarget[4], HEX);
    Serial.println(macAddrTarget[5], DEC);

    WakeOnLan::sendWOL(computer_ip, UDP, macAddrTarget, sizeof macAddrTarget);
    Serial.println("Notifying WOL packet sent successfully...");
    Serial.print(httpPost(String("http://") + PUB_HOST + "/controll/res.php?device=" + e_detail_device + "&shout=true&log=waking_up_by_request&state_changed=false", "application/json", String("{\"type\":\"wake_on_lan\", \"data\":\"Waking up ") + e_detail_device + "\", \"whisper\":" + e_detail_connid + "}"));
  }

  if (strcmp(ep_emitted, "@SERVER@") == 0) {
    connId = e_detail_connid;
    connSecret = (String)e_detail_secret;
    connTimeout = e_detail_timeout;
    runAliveLoop = true;
  }

}







///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////// SETUP //////////////////////////////////////////////

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  EEPROM.begin(4096);
  Serial.print("...STARTING...");
  Serial.println();

  pinMode(LED_BUILTIN, OUTPUT); // COMMENT THIS LINE IF YOUR PIN_LED_OUTPUT = LED_BUILTIN
  pinMode(PIN_LED_OUTPUT, OUTPUT); // Initialize as an output // To controll LED in this pin
  pinMode(PIN_LED_CTRL, INPUT);    // Initialize as an input // To toggle LED status manually and TOGGLE AP/STA+AP MODE (long press)

  setupWifiConfigServer(server, EEPROM_ADDR_CONNECTED_SSID, EEPROM_ADDR_CONNECTED_PASSWORD);

  /*** START SERVER ANYWAY XD ***/
  // Serial.println("Starting server anyway xD ...");
  // ESP_AP_STA(server, AP_SSID, AP_PASSWORD);
  /******************************/
  ESP_STATION(server, true); // Start in AP mode

  setLedModeInverted(true);
}





///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////// LOOP //////////////////////////////////////////////

void loop() {
  // put your main code here, to run repeatedly:

  handleNoPollSubscription(sub_WiFiclient, SUB_HOST, SUB_PORT, SUB_PATH, "POST", String("{\"clid\":\"") + clid + "\",\"ep\":[\"controll/wol_controller/" + clid + "/req\"]}", String(clid) + "/2022", doInLoop, [](){}, onParsed);
  
}
