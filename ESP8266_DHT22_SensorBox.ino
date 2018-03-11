#include <EEPROM.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <FS.h>

// CH340 Chipset Driver
// https://github.com/adrianmihalko/ch340g-ch34g-ch34x-mac-os-x-driver
//
//  Add Board Manager URL in Arduino IDE
//  http://arduino.esp8266.com/stable/package_esp8266com_index.json
//


struct configuration {
  char vers[32];
  // Influx variables
  char influx_server[64];
  char influx_port[16];
  char influx_db[64];
  char influx_location[64];
  char influx_measurement[64];

  //WebServer variables
  char httppw[32];
  
  // WiFi variables
  char host[64];
  char ssid[32];
  char pass[64];
};

// GLOBALS
//

// Delay after serial write
int DELAY = 2000;

// Configuration Struct
// version, influx server, influx port, influx database, sensor location, influx measurement, http password, wifi hostname, wifi ssid, wifi password
configuration defaults = { "0.0.1", "Host", "8086", "general", "location", "ambient", "foobar", "esp-dht-1", "SightUnseenFarm", "PASSWORD" };
configuration conf;

// Influx/HTTPClient globals
String influx_url;
HTTPClient http;

// HTTP Server Globals
ESP8266WebServer server(8080); // Set the HTTP Server port here
const String OKredirect = "<html><head><meta http-equiv='Refresh' content='3;url=/'></head><body>Updating settings...</body></html>";
const String ERRredirect = "<html><head><meta http-equiv='Refresh' content='3;url=/'></head><body>No form data received.</body></html>";

// WiFi Globals
#define ATTEMPTS 5
String apssid = "ESP-" + String(ESP.getChipId());
const char *appass = "password";
String ipaddr;
bool wifi_connected;

// DHT22 Globals
#define DHTPWR D3
#define DHTPIN D4
#define DHTTYPE DHT22
String cur_temp;
String cur_humidity;
String cur_hindex;
DHT dht(DHTPIN, DHTTYPE);

void sensorOn() {
  // Main Sensor Turn On
  pinMode(DHTPWR, OUTPUT);
  digitalWrite(DHTPWR, 1);
}

void buildInfluxUrl() {
  influx_url = "http://" + 
    String(conf.influx_server) + ":" + 
    String(conf.influx_port) +
    "/write?db=" + 
    String(conf.influx_db);
}

void readConfig() {
  if (EEPROM.read(511) != 0b01010101) {
    conf = defaults;
    EEPROM.write(511, 0b01010101);
    EEPROM.commit();
    writeConfig();
  }
  Serial.println(EEPROM.read(511), HEX);
  // Read eeprom into a configuration struct
  EEPROM.get(1, conf);
}

void writeConfig() {
  EEPROM.put(1, conf);
  EEPROM.commit();
}

void serialPrint(String line1, String line2="", String line3="", uint16_t delay_time=DELAY) {
  // It's split into lines because I had previously written this to support an oled display
  Serial.println(line1);
  Serial.println(line2);
  Serial.println(line3);
  delay(delay_time);
}

void wifiInit() {
  WiFi.mode(WIFI_STA);
  WiFi.hostname(conf.host);
  WiFi.begin(conf.ssid, conf.pass);
  
  // Try ATTEMPTS times to get on the network
  for (int x=0; x<ATTEMPTS; x++) {
    if (WiFi.status() != WL_CONNECTED) {
      serialPrint("Connecting to","WiFi...", "Attempt: " + String(x));
      serialPrint("Mac Address: " + WiFi.macAddress());
      delay(5000);
    } else {
      break;
    }
  } 
  
  // If wifi failed (60 seconds with no wifi)
  if (WiFi.status() != WL_CONNECTED) {
    serialPrint("Wifi failed", "Starting AP...");
    // Start the access point
    WiFi.softAP(apssid.c_str(), appass);
    ipaddr = WiFi.softAPIP().toString();
    serialPrint("AP:" + apssid, "Web config IP:", "http://" + WiFi.softAPIP().toString() + ":8080");
    wifi_connected = false;
  } else {
    ipaddr = "IP: " + WiFi.localIP().toString();
    serialPrint("Connected to: ", WiFi.SSID(), ipaddr);
    wifi_connected = true;
  }
}

void influxInit() {
  // Build the influx url
  buildInfluxUrl();
  
  // Start the http connection
  http.begin(influx_url);
  
  // Print the influx server info
  serialPrint("Influx Server: ", String(conf.influx_server), "Influx Port: " + String(conf.influx_port));
}

uint16_t influxwrite() {
  // Build the POST
  String influxout;
  influxout = "ambient,host=" + String(conf.host) + 
              ",location=" + String(conf.influx_location) + 
              " temperature=" + cur_temp + 
              ",humidity=" + cur_humidity + 
              ",heat_index=" + cur_hindex;

//  serialPrint("Influx Server: ", String(conf.influx_server), "Influx Port: " + String(conf.influx_port));
//  serialPrint("Influx out: ", String(influxout));
              
  // POST the POST and return the result
  return http.POST(influxout);
}

void resetsensor() {
  digitalWrite(DHTPWR, 0);
  digitalWrite(DHTPWR, 1);
  serialPrint("Resetting", "Sensor...");
  delay(2000);
  dht.begin();
}

void httpReturn(uint16_t httpcode, String mimetype, String content) {
  server.sendHeader("Connection", "close");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(httpcode, mimetype, content);
}

void htmlRoot() {
  // Build index.html
  String indexHtml = "<!DOCTYPE html>"
  "<html>"
  "  <head>"
  "  </head>"
  "  <style>"
  "  table, th, td {"
  "      border: 1px solid black;"
  "  }"
  "  </style>"
  "  <body>"
  "    <h1><b>Sensor Name: " + String(conf.host) + "</b></h1>"
  "    <h1><b>Sensor MAC Address: " + String(WiFi.macAddress()) + "</b></h1>"
  "    <h2><b>Current Readings:</b></h2>"
  "    <table>"
  "      <tr><td>Temperature</td><td>" + cur_temp + "</td></tr>"
  "      <tr><td>Humidity</td><td>" + cur_humidity + "<br></td></tr>"
  "      <tr><td>Heat Index</td><td>" + cur_hindex + "</td></tr>"
  "    </table>"
  "    <form action=\"/chgwifi\"  method=\"POST\">"
  "        <P>"
  "        <h2><b>Change WiFi Network</b></h2>"
  "        <table>"
  "          <tr><td>SSID</td><td><input name=\"input_ssid\" type=\"text\" value=\"" + String(conf.ssid) + "\"></td></tr>"
  "          <tr><td>Passphrase:</td><td><input name=\"input_pass\" type=\"password\" value=\"********\"></td></tr>"
  "        </table>"
  "        <input type=\"submit\" value=\"Save\">"
  "        </P>"
  "    </form>"
  "    <form action=\"/cfg_influx\"  method=\"POST\">"
  "      <h2><b>InfluxDB Configuration:</b></h2>"
  "      <P>"
  "      <table>"
  "        <tr><td>Server</td><td><input name=\"input_server\" type=\"text\" value=\"" + String(conf.influx_server) + "\"></tr>"
  "        <tr><td>Port</td><td><input name=\"input_port\" type=\"text\" value=\"" + String(conf.influx_port) + "\"></tr>"
  "        <tr><td>DB</td><td><input name=\"input_db\" type=\"text\" value=\"" + String(conf.influx_db) + "\"></tr>"
  "        <tr><td>Measurement</td><td><input name=\"input_measurement\" type=\"text\" value=\"" + String(conf.influx_measurement) + "\"></tr>"
  "        <tr><td>Location</td><td><input name=\"input_location\" type=\"text\" value=\"" + String(conf.influx_location) + "\"></tr>"
  "        <tr><td>Hostname</td><td><input name=\"input_host\" type=\"text\" value=\"" + String(conf.host) + "\"></tr>"
  "      </table>"
  "      <input type=\"submit\" value=\"Save\">"
  "      </P>"
  "    </form>"
  "  </body>"
  "</html>";
  httpReturn(200, "text/html", indexHtml);
}



void handleWebRequests(){
  serialPrint("handleWebRequests: " +  server.uri());
  if(loadFromSpiffs(server.uri())) return;
  
  String message = "File Not Detected\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " NAME:"+server.argName(i) + "\n VALUE:" + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  Serial.println(message);
  serialPrint(message);
}

bool loadFromSpiffs(String path){
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

  serialPrint("loadFromSpiffs path:" + path + " dataType: " + dataType);
  
  File dataFile = SPIFFS.open(path.c_str(), "r");
  if (server.hasArg("download")) dataType = "application/octet-stream";
  if (server.streamFile(dataFile, dataType) != dataFile.size()) {
    serialPrint("loadFromSpiffs != dataFile.size()" );
  }
 
  dataFile.close();
  return true;
}

void chgWifi() {
  if (server.args() != 2) {
    httpReturn(204, "text/html", ERRredirect);
  } else {
    strcpy(conf.ssid, server.arg("input_ssid").c_str());
    strcpy(conf.pass, server.arg("input_pass").c_str());
    httpReturn(200, "text/html", "<html><body>WiFi Settings Changed, see you on " + String(conf.ssid) + "!</body></html>");
    // Reconfigure WiFi
    serialPrint("WiFi settings", "changed!");
    writeConfig();
    wifiInit();
  }
}

void clearConfig() {
  // Wipe the config
  for (int x=5; x>0; x--) {
    serialPrint("Clearing config", "in " + String(x), "Reset to stop");
    delay(1000);
  }
  conf = defaults;
  writeConfig();
  httpReturn(200, "text/html", OKredirect);
  ESP.restart();
}

void cfgInflux() {
  if (server.args() != 6) {
    httpReturn(204, "text/html", ERRredirect);
  } else {
    strcpy(conf.influx_server, server.arg("input_server").c_str());
    strcpy(conf.influx_port, server.arg("input_port").c_str());
    strcpy(conf.influx_db, server.arg("input_db").c_str());
    strcpy(conf.influx_measurement, server.arg("input_measurement").c_str());
    strcpy(conf.influx_location, server.arg("input_location").c_str());
    strcpy(conf.host, server.arg("input_host").c_str());
    buildInfluxUrl;
    httpReturn(200, "text/html", OKredirect);
    writeConfig();
    influxInit();
  }
}

void httpInit() {
  MDNS.begin(conf.host);
  server.on("/", HTTP_GET, handleWebRequests);
  server.on("/chgwifi", HTTP_POST, chgWifi);
  server.on("/cfg_influx", HTTP_POST, cfgInflux);
  server.on("/clear_settings", HTTP_GET, clearConfig);
  server.onNotFound(handleWebRequests);
  server.begin();
  MDNS.addService("http", "tcp", 8080);
}

void setup() {
  // Start serial
  Serial.begin(115200);
  
  // init EEPROM, all 512 bytes
  EEPROM.begin(512);
  
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
  
  // Initialize the influxdb connection
  influxInit();
}

void loop() {
  // Loop 5 times, then display the ip address and conf.hostname
  for(int i=0; i < 5; i++) {
    float temp = dht.readTemperature(true);
    float humidity = dht.readHumidity();

    if (isnan(humidity) || isnan(temp)) {
      // Successfully failed to get a readout from the DHT22
      serialPrint("DHT22 Error");
      // Try to reset the sensor
      resetsensor();
      delay(3000);
    } else {
      // Successfully got a readout
      float hindex = dht.computeHeatIndex(temp, humidity);
      // Set the current globals for stuff.
      cur_temp = String(temp, 2);
      cur_humidity = String(humidity, 2);
      cur_hindex = String(hindex, 2);

      // Write out to serial
      serialPrint("Temp:          " + cur_temp + "F   ", 
                "Humidity:   " + cur_humidity + "% ",
                "Heat Index:  " + cur_hindex,
                1);


                
      // Handle HTTP requests for 10 seconds
      for(int x=0; x < 20; x++) {
        server.handleClient();
        delay(500);
      }

      if (WiFi.status() != 3 || wifi_connected == false) {
        // If we don't have wifi signal, whine about it.
        serialPrint("No Wifi", "AP: " + apssid, "IP: " + ipaddr);
        serialPrint("Mac Address: " + WiFi.macAddress());
      } else {  
        uint16_t httpCode = influxwrite();
        // Parse the return
        // Apparently HTTP Code 204 is successful for influxDB.  Would be nice
        // if they didn't use an error code for OK
        if (httpCode != HTTP_CODE_OK && httpCode != 204) { 
          serialPrint("INFLUX HTTP", "ERROR: " + String(httpCode), "");
        }
      }
    }
  }
  // Print the Hostname and IP address
  serialPrint(ipaddr, "Host:", String(conf.host), 5000);
}

