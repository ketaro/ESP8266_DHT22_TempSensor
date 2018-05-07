#include <DHT.h>
#include <EEPROM.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <FS.h>

// CH340 Chipset Driver
// https://github.com/adrianmihalko/ch340g-ch34g-ch34x-mac-os-x-driver
//
// Add Board Manager URL in Arduino IDE
// http://arduino.esp8266.com/stable/package_esp8266com_index.json
//
// Plugin for uploading SPIFFS
// https://github.com/esp8266/arduino-esp8266fs-plugin
//
// Web Server / SPIFFS inspired by
// https://circuits4you.com/2018/02/03/esp8266-nodemcu-adc-analog-value-on-dial-gauge/
//

#define INO_VERSION              "0.3.0"
#define CONFIG_VERSION           3
#define EEPROM_SIZE              512
#define EEPROM_CONFIG_START      0
#define DEFAULT_HOSTNAME         "esp-dht-1"
#define DEFAULT_HTTP_PORT        8080
#define DEFAULT_SAMPLE_INTERVAL  60
#define DEFAULT_SSID             "SightUnseenFarm"
#define DEFAULT_WIFI_PW          "PASSWORD"

#define DB_TYPE_NONE       0
#define DB_TYPE_INFLUXDB   1
#define DB_TYPE_POSTGRESQL 2


// EEPROM Configuration Structure
struct configuration {
  unsigned int version;

  // System Settings
  char hostname[20];
  char location[20];
  unsigned short http_server_port;
  char http_pw[20];

  // Network Settings
  char ssid[32];
  char wifi_pass[64];

  // Database Settings
  byte           db_type;             // 0 - none, 1 - influxdb
  char           db_host[64];
  unsigned short db_port;
  char           db_name[20];
  char           db_measurement[15];  // Measurement string

  unsigned int sample_interval;

};

// Configuration Struct
configuration defaults = { CONFIG_VERSION, DEFAULT_HOSTNAME, "unknown", DEFAULT_HTTP_PORT, "",  
                           DEFAULT_SSID, DEFAULT_WIFI_PW,
                           DB_TYPE_INFLUXDB, "influxdb", 8086, "temp", "ambient", DEFAULT_SAMPLE_INTERVAL  };
configuration conf;



//
// GLOBALS
//

// DHT22 Globals
#define DHTPWR D3
#define DHTPIN D4
#define DHTTYPE DHT22

int poll_sensor_interval   = 5000;    // 5 seconds
int next_sensor_poll       = poll_sensor_interval;
int send_to_db_interval    = 30000;    // 30 seconds
int next_send_to_db        = send_to_db_interval;
int network_check_interval = 60*5000;  // 5 minutes
int next_network_check     = network_check_interval;

// Influx/HTTPClient globals
String influx_url;
HTTPClient http;

// HTTP Server Globals
ESP8266WebServer server(8080); // Set the HTTP Server port here

// WiFi Globals
#define ATTEMPTS 5
String apssid = "ESP-" + String(ESP.getChipId());
const char *appass = "password";
String ipaddr;
bool wifi_connected;

unsigned long lastSensorRead = 0;
String cur_temp;
String cur_humidity;
String cur_hindex;
DHT dht(DHTPIN, DHTTYPE);



//
// C O N F I G S
//
void readConfig() {
  unsigned int current_version;
  
  Serial.print( "READING CONFIG FROM EEPROM.  VERSION: " );

  // Read the config version number from eeprom
  EEPROM.get( EEPROM_CONFIG_START, current_version );
  Serial.println( current_version, DEC );

  // If config version is not something we can upgrade from,
  // set a default config.
  if ( current_version != 3 ) {
    Serial.println( "Unsupported or no config found in EEPROM.  Resettings to defaults." );
    conf = defaults;
    writeConfig();
  }

  // Read EEPROMP into config struct
  EEPROM.get( EEPROM_CONFIG_START, conf );
}

// Save the running config
void writeConfig() {
  Serial.print( "Clearing EEPROM, size: " );
  Serial.println( EEPROM.length() );
  for (int i = 0 ; i < EEPROM.length() ; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  
  Serial.println( "Writing config to EEPROM" );
  EEPROM.put( EEPROM_CONFIG_START, conf );
  EEPROM.commit();
}


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
  WiFi.hostname( conf.hostname );
  WiFi.begin( conf.ssid, conf.wifi_pass );
  wifi_connected = false;

  // Try *attempts* times to get on the network
  Serial.println( "Mac Address: " + WiFi.macAddress() );
  Serial.print( "Connecting to WiFi (" + String(conf.ssid) + ")" );
  for ( int x=0; x < attempts; x++ ) {
    if ( WiFi.status() == WL_CONNECTED )
      break;
    
    Serial.print(".");
    delay(5000);
  }
  Serial.println();

  if ( WiFi.status() == WL_CONNECTED ) {
    ipaddr = "IP: " + WiFi.localIP().toString();
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
// I N F L U X D B
//
void buildInfluxUrl() {
  influx_url = "http://" + 
    String(conf.db_host) + ":" + 
    String(conf.db_port) +
    "/write?db=" + 
    String(conf.db_name);
}

void influxInit() {
  // Build the influx url
  buildInfluxUrl();
  
  // Start the http connection
  http.begin(influx_url);
  
  // Print the influx server info
  Serial.println("Influx Server: " + String(conf.db_host) + ":" + String(conf.db_port));
}


uint16_t influxDBSend() {
  // Build the POST
  String influxout;
  influxout = "ambient,host=" + String(conf.hostname) + 
              ",location=" + String(conf.location) + 
              " temperature=" + cur_temp + 
              ",humidity=" + cur_humidity + 
              ",heat_index=" + cur_hindex;

  Serial.println( "[InfluxDB] " + String(conf.db_host) + ":" + String(conf.db_port) );
  Serial.println( "[InfluxDB] " + String(influxout) );
              
  // POST the POST and return the result
  return http.POST(influxout);
}


// Send sensor readings to database
// (if the network is available!)
void sendToDatabase() {
  if (!networkOK()) {
    Serial.println("[NO NETWORK] Cannot send to DB.  AP: " + apssid +
                   "  IP: " + ipaddr + " (" + WiFi.macAddress() + ")" );
    return;
  }

  // TODO: Eventually this can be a DB type check
  // when we support more thing that just InfluxDB  
  uint16_t httpCode = influxDBSend();

  // Parse the return
  // Apparently HTTP Code 204 is successful for influxDB.
  if (httpCode != HTTP_CODE_OK && httpCode != 204) {
    Serial.println( "INFLUX HTTP ERROR: " + String(httpCode) );
  }

}



//
// W E B S E R V E R
//

void httpInit() {
  Serial.println( "HTTP INIT, hostname: " + String( conf.hostname ) + "  port: " + String( conf.http_server_port ) );
//  MDNS.begin( conf.hostname );
  server.on("/",         HTTP_GET,  handleWebRequests);
  server.on("/config",   HTTP_GET,  jsonConfigData);
  server.on("/network",  HTTP_POST, processNetworkSettings);
  server.on("/reset",    HTTP_POST, processConfigReset);
  server.on("/sensors",  HTTP_GET,  jsonSensorData);
  server.on("/settings", HTTP_POST, processSettings);
  server.onNotFound(handleWebRequests);
  
  server.begin();
//  MDNS.addService( "http", "tcp", conf.http_server_port );
}


void handleWebRequests(){
  Serial.println( "handleWebRequests: " +  server.uri() );
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
  bool wifiPassSaved = false;

  if (String(conf.wifi_pass).length() > 0)
    wifiPassSaved = true;
  
  
  String jsonstr = "{\"ver\": \"" + String(conf.version) + "\", " 
                   "\"ino_ver\": \"" + INO_VERSION + "\", "
                   "\"mac\": \"" + String(WiFi.macAddress()) + "\", "
                   "\"hostname\": \"" + String(conf.hostname) + "\", "
                   "\"db\": {"
                       "\"dbname\": \"" + String(conf.db_name) + "\", "
                       "\"host\": \"" + String(conf.db_host) + "\", "
                       "\"location\": \"" + String(conf.location) + "\", "
                       "\"measurement\": \"" + String(conf.db_measurement) + "\", "
                       "\"type\": \"influx\", "
                       "\"port\": \"" + String(conf.db_port) + "\""
                       "}, "
                   "\"net\": {"
                       "\"ssid\": \"" + String(conf.ssid) + "\", "
                       "\"pw\": \"" + String(wifiPassSaved) + "\""
                       "}"
                   "}";

  httpReturn(200, "application/json", jsonstr);

}


// POST /reset
void processConfigReset() {
  // Delay to give the user a chance to pull the plug
  for (int x=5; x>0; x--) {
    Serial.println("[RESET REQUEST] Clearing config in " + String(x) + "...Reset to stop");
    delay(1000);
  }

  // Write defaults to the config
  conf = defaults;
  writeConfig();
  
  httpReturn(200, "application/json", "{\"status\": \"ok\"}");

  // Restart
  ESP.restart();
}


// POST /settings
void processSettings() {
  if (server.args() < 6) {
    httpReturn(400, "text/html", "Missing Data");
    return;
  }
  
  // Update config with values from form
  // TODO: Better error checking
  strcpy(conf.db_host,          server.arg("db_host").c_str());

  // TODO: CONVERT STRING TO INT
  //strcpy(conf.db_port,          server.arg("db_port").c_str());
  strcpy(conf.db_name,          server.arg("db_name").c_str());
  strcpy(conf.db_measurement,   server.arg("db_measurement").c_str());
  strcpy(conf.location,         server.arg("location").c_str());
  writeConfig();

  // Update the influx URL and re-init the Influx connection
  buildInfluxUrl();
  influxInit();

  // Success to the client.
  httpReturn(200, "application/json", "{\"status\": \"ok\"}");
}


// POST /network
void processNetworkSettings() {
  if (server.args() < 2) {
    httpReturn(400, "text/html", "Missing Data");
    return;
  }

  // Update config with values from form
  // TODO: Better error checking
  strcpy( conf.ssid, server.arg("ssid").c_str() );
  strcpy( conf.hostname, server.arg("hostname").c_str() );

  // Only change the wifi password if one was passed
  if ( server.arg("wifi_pw").length() > 0 )
    strcpy( conf.wifi_pass, server.arg("wifi_pw").c_str() );

  writeConfig();

  // Success to the client.
  httpReturn( 200, "application/json", "{\"status\": \"ok\"}" );

  // Reconfigure WiFi
  Serial.println( "WiFi settings changed!" );
  wifiInit();
}




//
// A R D U I N O  S E T U P
void setup() {
  delay(1000);
  
  // Start serial
  Serial.begin(115200);
  Serial.println();
  
  // init EEPROM, all 512 bytes
  EEPROM.begin( EEPROM_SIZE );

  // Initialize File System
  SPIFFS.begin();
  Serial.println("File System Initialized");
  
  // Read in the config
  readConfig();

  // Turn on DHT22
  sensorOn();
  
  // Initialize WiFi
  wifiInit();

  // Initialize the web server
  httpInit();

  // Start the temperature sensor
  dht.begin();
/*
  // Initialize the influxdb connection
  influxInit();
*/
}


// Main Arduino Loop
void loop() {

  // note: this will overflow after ~50 days
  unsigned long now = millis();

  if (now > next_sensor_poll) {   // Time to read the sensors
    readSensors();
    next_sensor_poll = now + poll_sensor_interval;
  }

/*
  if (now > next_send_to_db) {  // Time to send readings to the db
    sendToDatabase();
    next_send_to_db = now + send_to_db_interval;
  }
*/
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


