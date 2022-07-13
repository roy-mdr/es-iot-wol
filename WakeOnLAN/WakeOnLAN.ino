/*****************************************************************************/
/*****************************************************************************/
/*               STATUS: WORKING                                             */
/*            TESTED IN: ESP-01                                              */
/*                   AT: 2022.01.18                                          */
/*     LAST COMPILED IN: KAPPA                                               */
/*****************************************************************************/
/*****************************************************************************/

#define clid "wol_ES"




#include <ESP8266HTTPClient.h> // Uses core ESP8266WiFi.h internally
#include <config_wifi_roy.h>
#include "WakeOnLan.h"



WiFiUDP UDP;
/**
 * The Magic Packet needs to be sent as BROADCAST in the LAN
 */
IPAddress computer_ip(255,255,255,255); 

/**
 * The targets MAC address to send the packet to
 */
byte mac_0[] = { 0x30, 0x9C, 0x23, 0xAA, 0x5E, 0x1B };
byte mac_1[] = { 0x1C, 0x1B, 0x0D, 0x9E, 0xE0, 0x49 };

void sendWOL();



HTTPClient http;

ESP8266WebServer server(80);
WiFiServer wifiserver(80);
WiFiClient wifiClient;

#define EEPROM_ADDR_CONNECTED_SSID 1       // Start saving connected network SSID from this memory address
#define EEPROM_ADDR_CONNECTED_PASSWORD 30  // Start saving connected network Password from this memory address
#define AP_SSID clid                       // Set your own Network Name (SSID)
#define AP_PASSWORD "12345678"             // Set your own password

#define PIN_LED_OUTPUT 5 // Pin de LED que va a ser controlado // before was LED_BUILTIN instead of 5
#define PIN_LED_CTRL 15 // Pin que cambia/constrola el estado del LED en PIN_LED_OUTPUT manualmente (TOGGLE LED si cambia a HIGH), si se presiona mas de 2 degundos TOGGLE el modo AP y STA+AP

byte PIN_LED_CTRL_VALUE;

//const char* host = "estudiosustenta.myds.me";
//const char* notiHost = "notify.estudiosustenta.myds.me";
//const int port = 80;
const char* host = "192.168.1.72";
const char* notiHost = "192.168.1.72";
const int port = 1010;
const char* path = "/";

// Declaramos o instanciamos un cliente que se conectará al Host
WiFiClient client;

/***** SET CONNECTION TIMEOUT *****/
unsigned long to_timestamp = millis();
unsigned int  to_track = 0;
unsigned int  to_times = 0;
unsigned int  to_timeout = 60 * 1000; // 60 seconds

/***** SET PIN_LED_CTRL TIMER TO PRESS *****/
unsigned long lc_timestamp = millis();
unsigned int  lc_track = 0;
unsigned int  lc_times = 0;
unsigned int  lc_timeout = 2 * 1000; // 2 seconds





///////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////// CONNECTION ///////////////////////////////////////////

/////////////////////////////////////////////////
/////////////////// HTTP GET ///////////////////

// httpGet("http://192.168.1.80:69/");
String httpGet(String url) {

  String response;

  http.begin(wifiClient, url.c_str());
  
  // Send HTTP GET request
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
            ESP_STATION(server);
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
        
        // TOGGLE LED STATUS
        digitalWrite(PIN_LED_OUTPUT, !digitalRead(PIN_LED_OUTPUT));
      
        Serial.print("LED status toggled via pin. Now LED is: ");
        Serial.println(digitalRead(PIN_LED_OUTPUT));
      
        if (WiFi.status() == WL_CONNECTED) {

          String led_state_string = "error";

          if( digitalRead(PIN_LED_OUTPUT) == 1 ) {
            led_state_string = "true";
          } else if( digitalRead(PIN_LED_OUTPUT) == 0 ) {
            led_state_string = "false";
          }
        
          Serial.println("Notifying LED was changed manually...");
          httpGet(String("http://") + host + "/test/WeMosServer/controll/response.php?set=" + led_state_string + "&info=pin_change_on_device&clid=" + clid);
        }
      }
    }
  }
  

}

/////////////////////////////////////////////////
//////////////// GET LAST STATUS ////////////////

bool requestLast() {

  Serial.println("Getting last value from database...");

  String lastValue = httpGet(String("http://") + host + "/test/WeMosServer/controll/getLast.php");
  Serial.print("Last value: ");
  Serial.print(lastValue);

  if ( lastValue == "1" ) {
    Serial.println(" (on)");
    digitalWrite(PIN_LED_OUTPUT, HIGH);   // Turn the LED on
    Serial.println("Notifying we got the last value correctly...");
    httpGet(String("http://") + host + "/test/WeMosServer/controll/response.php?set=true&info=device_checked_last_status&clid=" + clid);
    return true;
  } else if ( lastValue == "0" ) {
    Serial.println(" (off)");
    digitalWrite(PIN_LED_OUTPUT, LOW);   // Turn the LED off
    Serial.println("Notifying we got the last value correctly...");
    httpGet(String("http://") + host + "/test/WeMosServer/controll/response.php?set=false&info=device_checked_last_status&clid=" + clid);
    return true;
  } else {
    Serial.println();
    Serial.print("CAN NOT CONNECT TO HOST: ");
    Serial.println(host);
    return false;
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
  ESP_STATION(server); // Start in AP mode

  setLedModeInverted(true);
}





///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////// LOOP //////////////////////////////////////////////

void loop() {
  // put your main code here, to run repeatedly:

  /***** FUNCTIONS IN LOOP *****/
  doInLoop();
  /***** ----------------- *****/
  //Serial.println("loop_doInLoop");
  if (WiFi.status() == WL_CONNECTED) {
    // Do while WiFi is connected
    
    Serial.printf("\n[Connecting to %s ... ", notiHost);
    // Intentamos conectarnos
    if (client.connect(notiHost, port)) {
      Serial.println("connected]");

      requestLast();

      String dataPOST = String("{\"clid\":\"") + clid + "\",\"ep\":[\"controll/wol_controller/wol_ES/req\"]}";
      Serial.println("[Sending POST request]");
      
      ///// ---------- BEGIN
      client.print("POST ");
      client.print(path);
      client.println(" HTTP/1.1");
      
      client.print("Host: ");
      client.println(notiHost);
      
      client.println("User-Agent: wol_ES/2022");
      
      client.println("Accept: */*");
      
      client.println("Content-Type: application/json");

      client.print("Content-Length: ");
      client.println(dataPOST.length());
      client.println();
      client.print(dataPOST);

      Serial.println("[Response:]");

      bool headersDone = false;
      bool readStr = false;

      // Mientras la conexion perdure
      while (client.connected()) {

        if ( millis() != to_timestamp ) { // "!=" intead of ">" tries to void possible bug when millis goes back to 0
          to_track++;
          to_timestamp = millis();
        }

        if ( to_track > to_timeout ) {
          // DO TIMEOUT!
          to_times++;
          to_track = 0;
          Serial.print("[timeout #");
          Serial.print(to_times);
          Serial.println("]");
          client.stop();
          
          if (to_times > 3) {
            
            to_times = 0;
            
            if (WiFi.getMode() != WIFI_AP_STA) {
              Serial.println("restarting...");
              ESP.restart(); // tells the SDK to reboot, not as abrupt as ESP.reset()
            }
            
          }
        }

        /***** SAME FUNCTIONS IN LOOP *****/
//yield(); // = delay(0);
//Serial.println("loop_client_connected");
        doInLoop();
        /***** because this while loop hogs the loop *****/

        // Si existen datos disponibles
        if (client.available()) {
          
//Serial.println("loop_client_available");

          to_track = 0;
          //to_times = 0;

          String line = client.readStringUntil('\n');
          Serial.println(line);
          
          if ( (line.length() == 1) && (line[0] == '\r') ) {
            if (headersDone) {
              client.stop();
            } else {
              Serial.println("---[HEADERS END]---");
              headersDone = true;
              readStr = false;
            }
          } else {
            if (headersDone) {
              if (readStr) {
                ///// START LOGIC
                Serial.println("^^^^^");

                if (line[2] != '0') { // PREVENT BUG WHEN NOOP SIGNAL IS SENT (''0'')
                
                  /*if (line[line.length() - 4] == '0') {
                    Serial.println("Servidor pide cambiar LED a estado: off");
                    digitalWrite(PIN_LED_OUTPUT, LOW);   // Turn the LED on
                    Serial.println("Notifying LED status changed successfully...");
                    httpGet(String("http://") + host + "/test/WeMosServer/controll/response.php?set=false&info=changed_by_request&clid=" + clid);
                  }*/

                  if (line[line.length() - 5] == '0') {
                    Serial.println("Servidor pide despertar a OMEGA");
                    WakeOnLan::sendWOL(computer_ip, UDP, mac_0, sizeof mac_0);
                    Serial.println("Notifying WOL packet sent successfully...");
                    httpGet(String("http://") + host + "/test/WeMosServer/controll/response.php?set=true&info=waking_up_PHI&clid=" + clid);
                    httpPost(String("http://") + host + "/controll/res.php", "application/json", "\"ok\"");
                  }

                  if (line[line.length() - 5] == '1') {
                    Serial.println("Servidor pide despertar a KAPPA");
                    WakeOnLan::sendWOL(computer_ip, UDP, mac_1, sizeof mac_1);
                    Serial.println("Notifying WOL packet sent successfully...");
                    httpGet(String("http://") + host + "/test/WeMosServer/controll/response.php?set=true&info=waking_up_PHI&clid=" + clid);
                    httpPost(String("http://") + host + "/controll/res.php", "application/json", "\"ok\"");
                  }

                  /*if (line[line.length() - 5] == 'x') {
                    Serial.print("Servidor solicita el estado actual del LED. Estado actual: ");
                    Serial.println(digitalRead(PIN_LED_OUTPUT));
                    if (digitalRead(PIN_LED_OUTPUT) == 1) {
                      httpGet(String("http://") + host + "/test/WeMosServer/controll/response.php?set=true&info=current_status_requested&clid=" + clid);
                    }
                    if (digitalRead(PIN_LED_OUTPUT) == 0) {
                      httpGet(String("http://") + host + "/test/WeMosServer/controll/response.php?set=false&info=current_status_requested&clid=" + clid);
                    }

                  }*/
/* PARA COMPROBAR QUE PASA SI SE REINICIA EL MODULO Y EN EL INTER CAMBIA EL ESTADO (EN WEB)
Serial.println("restarting...");
ESP.restart(); // tells the SDK to reboot, not as abrupt as ESP.reset()
*/
                } else {
                  Serial.println("(NOOP)");
                }
                
              }
              readStr = !readStr;
            }


          }
        }
      }
      ///// ---------- END
      
      // Una vez el servidor envia todos los datos requeridos se desconecta y el programa continua
      Serial.println("\n[Disconnecting...]");
      client.stop();
      Serial.println("\n[Disconnected]");
    } else {
      Serial.println("connection failed!]");
      client.stop();
    }

  } else {
    // Do while WiFi is NOT connected
    //Serial.println("disconnected");
  }
  
}
