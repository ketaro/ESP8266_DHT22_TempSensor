/*
 *  ESP8266+DHT22 Temperature Sensor
 *  Nick Avgerinos - http://github.com/ketaro
 *  Tyler Eaton - https://github.com/googlegot
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 *  CH340 Chipset Driver (for OSX)
 *  https://github.com/adrianmihalko/ch340g-ch34g-ch34x-mac-os-x-driver
 *
 *  Add Board Manager URL in Arduino IDE
 *  http://arduino.esp8266.com/stable/package_esp8266com_index.json
 *
 *  Plugin for uploading SPIFFS
 *  https://github.com/esp8266/arduino-esp8266fs-plugin
 *
 *  Web Server / SPIFFS inspired by:
 *  https://circuits4you.com/2018/02/03/esp8266-nodemcu-adc-analog-value-on-dial-gauge/
 * 
 *  OTA Update
 *  http://esp8266.github.io/Arduino/versions/2.0.0/doc/ota_updates/ota_updates.html
 *
 */

#include <DHT.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <FS.h>

#include "defaults.h"
#include "Config.h"
#include "DB.h"



//
// Helper Classes
Config config;
DB db;

//
// GLOBALS
//
int poll_sensor_interval   = 5000;    // 5 seconds
int next_sensor_poll       = poll_sensor_interval;
int send_to_db_interval    = 30000;    // 30 seconds
int next_send_to_db        = send_to_db_interval;
int network_check_interval = 60*5000;  // 5 minutes
int next_network_check     = network_check_interval;

// HTTP Server Globals
ESP8266WebServer        server(8080);   // Set the HTTP Server port here
ESP8266HTTPUpdateServer httpUpdater;  // OTA Update Service
const char* auth_realm    = "ESP8266 TempSensor";
String auth_fail_response = "Authentication Failed";

// WiFi Globals
#define ATTEMPTS 5
String apssid = "ESP-" + String(ESP.getChipId());
const char *appass = DEFAULT_WIFI_PW;
String ipaddr;
bool wifi_connected;

unsigned long lastSensorRead = 0;
String cur_temp;
String cur_humidity;
String cur_hindex;
DHT dht(DHTPIN, DHTTYPE);


//
// S E N S O R
//
void sensorOn() {
  // Main Sensor Turn On
  pinMode(DHTPWR, OUTPUT);
  digitalWrite(DHTPWR, 1);
}

void resetSensor() {
  digitalWrite(DHTPWR, 0);
  digitalWrite(DHTPWR, 1);
  
  Serial.println("Resetting Sensor...");
  delay(2000);
  
  dht.begin();
}

// Attempt to read sensor values from the DHT22
void readSensors() {
    float temp     = dht.readTemperature(true);
    float humidity = dht.readHumidity();

    if (isnan(humidity) || isnan(temp)) {
      // Successfully failed to get a readout from the DHT22
      Serial.println("DHT22 Error");
      
      // Try to reset the sensor, may the odds be ever in your favor.
      resetSensor();
      delay(3000);

      return;
    }
    
    // Successfully got a readout
    float hindex = dht.computeHeatIndex(temp, humidity);
      
    // Set the current globals for stuff.
    cur_temp     = String(temp, 2);
    cur_humidity = String(humidity, 2);
    cur_hindex   = String(hindex, 2);
    lastSensorRead = millis();

    // Write out to serial
    Serial.println("Temp: "        + cur_temp + "F   " 
                   "Humidity: "    + cur_humidity + "%  "
                   "Heat Index:  " + cur_hindex);
}

//
// N E T W O R K
//

void wifiInit() {
  // Try ATTEMPTS times to get on the network
  wifiConnect( ATTEMPTS );
  
  // If WiFi Connection failed in the first ATTEMPTS*5 seconds of
  // init, we'll start an AP
  if ( !networkOK() ) {
    Serial.println( "WiFi Connection failed, Starting AP..." );

    // Start the access point
    WiFi.mode( WIFI_AP );
    WiFi.softAP( apssid.c_str(), appass );
    ipaddr = WiFi.softAPIP().toString();
    Serial.println("AP:" + apssid + " Web config IP: http://" + WiFi.softAPIP().toString() + ":8080");
    wifi_connected = false;
  }
}


// Attempt to connect to the SSID info in our config.
void wifiConnect(int attempts) {
  WiFi.mode( WIFI_STA );
  WiFi.hostname( config.conf.hostname );
  WiFi.begin( config.conf.ssid, config.conf.wifi_pw );
  wifi_connected = false;

  // Try *attempts* times to get on the network
  Serial.println( "Mac Address: " + WiFi.macAddress() );
  Serial.print( "Connecting to WiFi (" + String(config.conf.ssid) + ")" );
  for ( int x=0; x < attempts; x++ ) {
    if ( WiFi.status() == WL_CONNECTED )
      break;
    
    Serial.print(".");
    delay(5000);
  }
  Serial.println();

  if ( WiFi.status() == WL_CONNECTED ) {
    ipaddr = WiFi.localIP().toString();
    Serial.println( "Connected to: " + WiFi.SSID() + "  IP: " + ipaddr );
    wifi_connected = true;
  }
}


// Returns true if the wifi is connected
bool networkOK() {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  return true;
}


//
// W E B S E R V E R
//

void httpInit() {
  Serial.println( "HTTP INIT, hostname: " + String( config.conf.hostname ) + "  port: " + String( config.conf.http_server_port ) );
//  MDNS.begin( conf.hostname );
  server.on("/",         HTTP_GET,  handleWebRequests);
  server.on("/config",   HTTP_GET,  jsonConfigData);
  server.on("/network",  HTTP_POST, processNetworkSettings);
  server.on("/reset",    HTTP_POST, processConfigReset);
  server.on("/sensors",  HTTP_GET,  jsonSensorData);
  server.on("/settings", HTTP_POST, processSettings);
  server.onNotFound(handleWebRequests);

  // Attach the OTA update service
  httpUpdater.setup( &server, HTTP_OTA_UPDATE_PATH, HTTP_AUTH_USER, config.conf.http_pw );
  
  server.begin();
//  MDNS.addService( "http", "tcp", conf.http_server_port );
}


bool authRequired() {
  // TEST_MODE disabled authentication
  if ( TEST_MODE ) return false;

  if ( !server.authenticate( HTTP_AUTH_USER, config.conf.http_pw ) ) {
    server.requestAuthentication( DIGEST_AUTH, auth_realm, auth_fail_response );
    return true;
  }
  return false;
}

void handleWebRequests(){
  Serial.println( "handleWebRequests: " +  server.uri() );

  if ( authRequired() ) return;  // Page requires authentication

  if ( loadFromSpiffs( server.uri() ) ) return;
  
  String message = "File Not Detected\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for ( uint8_t i = 0; i < server.args(); i++ ) {
    message += " NAME:"+server.argName(i) + "\n VALUE:" + server.arg(i) + "\n";
  }
  server.send( 404, "text/plain", message );
  Serial.println( message );
}


// Handle loading a file from the local file system (SPIFFS)
bool loadFromSpiffs( String path ){
  String dataType = "text/plain";
  if(path.endsWith("/")) path += "index.html";
 
  if(path.endsWith(".src")) path = path.substring(0, path.lastIndexOf("."));
  else if(path.endsWith(".html")) dataType = "text/html";
  else if(path.endsWith(".htm")) dataType = "text/html";
  else if(path.endsWith(".css")) dataType = "text/css";
  else if(path.endsWith(".js")) dataType = "application/javascript";
  else if(path.endsWith(".png")) dataType = "image/png";
  else if(path.endsWith(".gif")) dataType = "image/gif";
  else if(path.endsWith(".jpg")) dataType = "image/jpeg";
  else if(path.endsWith(".ico")) dataType = "image/x-icon";
  else if(path.endsWith(".xml")) dataType = "text/xml";
  else if(path.endsWith(".pdf")) dataType = "application/pdf";
  else if(path.endsWith(".zip")) dataType = "application/zip";

  Serial.println("loadFromSpiffs path:" + path + " dataType: " + dataType);
  
  File dataFile = SPIFFS.open(path.c_str(), "r");
  if (server.hasArg("download")) dataType = "application/octet-stream";
  if (server.streamFile(dataFile, dataType) != dataFile.size()) {
    Serial.println("loadFromSpiffs != dataFile.size()" );
  }
 
  dataFile.close();
  return true;
}


// Respond to a web client request
void httpReturn(uint16_t httpcode, String mimetype, String content) {
  server.sendHeader("Connection", "close");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(httpcode, mimetype, content);
}


// GET /sensors
// Return Sensor Values in a JSON string
void jsonSensorData() {
  String jsonstr = "{\"hum\": " + cur_humidity + ", \"hidx\": " + cur_hindex + ", \"temp\": " + cur_temp + "}";
  httpReturn(200, "application/json", jsonstr);
}


// GET /config
// Return a JSON string of the current settings
void jsonConfigData() {
  if ( authRequired() ) return;  // Page requires authentication
    
  String jsonstr = config.JSON( String(WiFi.macAddress()) );
  
  httpReturn(200, "application/json", jsonstr);

}


// POST /reset
void processConfigReset() {
  if ( authRequired() ) return;  // Page requires authentication

  // Delay to give the user a chance to pull the plug
  for (int x=5; x>0; x--) {
    Serial.println("[RESET REQUEST] Clearing config in " + String(x) + "...Reset to stop");
    delay(1000);
  }

  // Write defaults to the config
  config.resetConfig();
  
  httpReturn(200, "application/json", "{\"status\": \"ok\"}");

  // Restart
  ESP.restart();
}


// POST /settings
void processSettings() {
  if ( authRequired() ) return;  // Page requires authentication

//  if (server.args() < 6) {
//    httpReturn(400, "text/html", "Missing Data");
//    return;
//  }
  
  // Update config with values from form
  if ( server.hasArg("db_host") )        config.set( CONFIG_DB_HOST,         server.arg("db_host") );
  if ( server.hasArg("db_port") )        config.set( CONFIG_DB_PORT,         server.arg("db_port") );
  if ( server.hasArg("db_name") )        config.set( CONFIG_DB_NAME,         server.arg("db_name") );
  if ( server.hasArg("db_measurement") ) config.set( CONFIG_DB_MEASUREMENT,  server.arg("db_measurement") );
  if ( server.hasArg("location") )       config.set( CONFIG_LOCATION,        server.arg("location") );
  if ( server.hasArg("interval") )       config.set( CONFIG_SAMPLE_INTERVAL, server.arg("interval") );

  if ( server.hasArg("http_pw") )        config.set( CONFIG_HTTP_PW,         server.arg("http_pw") );

  config.writeConfig();

  // Tell the DB to re-init with new settings
  db.init( config );

  // Success to the client.
  httpReturn(200, "application/json", "{\"status\": \"ok\"}");

}


// POST /network
void processNetworkSettings() {
  if ( authRequired() ) return;  // Page requires authentication

  if (server.args() < 2) {
    httpReturn(400, "text/html", "Missing Data");
    return;
  }

  // Update config with values from form
  config.set( CONFIG_SSID, server.arg("ssid") );
  config.set( CONFIG_HOSTNAME, server.arg("hostname") );

  // Only change the wifi password if one was passed
  if ( server.arg("wifi_pw").length() > 0 )
    config.set( CONFIG_WIFI_PW, server.arg("wifi_pw") );

  // Success to the client.
  httpReturn( 200, "application/json", "{\"status\": \"ok\"}" );

  // Save the running config
  config.writeConfig();

  // Wait a bit before disconnecting/reconnecting
  delay(2000);

  // Disconnect the wifi and re-run the setup routine
  WiFi.disconnect( true );
  setup();
}




//
// A R D U I N O  S E T U P
void setup() {
  delay(1000);
  
  // Start serial
  Serial.begin(115200);
  Serial.println();
  
  // Initialize File System
  SPIFFS.begin();
  Serial.println("File System Initialized");
  
  // Turn on DHT22
  send_to_db_interval = config.conf.sample_interval * 1000;
  sensorOn();
  
  // Initialize WiFi
  wifiInit();

  // Initialize the web server
  httpInit();

  // Start the temperature sensor
  dht.begin();

  // Initialize the database library
  db.init( config );

}


// Main Arduino Loop
void loop() {

  // note: this will overflow after ~50 days
  unsigned long now = millis();

  if (now > next_sensor_poll) {   // Time to read the sensors
    readSensors();
    next_sensor_poll = now + poll_sensor_interval;
  }

  if (now > next_send_to_db) {  // Time to send readings to the db
    if (networkOK()) {  // Can't send if we're not connected
      db.send( cur_temp, cur_humidity, cur_hindex );
    } else {
      Serial.println("[NO NETWORK] Cannot send to DB.  AP: " + apssid +
                     "  IP: " + ipaddr + " (" + WiFi.macAddress() + ")" );
    }
    next_send_to_db = now + send_to_db_interval;
  }

  // Network check to see if we're still connected to wifi
  if ( now > next_network_check ) {
    if ( !networkOK() )
      wifiConnect( ATTEMPTS );

    if ( !networkOK() )
      WiFi.mode( WIFI_AP );

    next_network_check = now + network_check_interval;
  }

  // Handle any HTTP Requests
  server.handleClient();

}


