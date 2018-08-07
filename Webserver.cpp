//
// Webserver.cpp - Library for wrapping the ESP8266 Web Server
//                 also manages the SPIFFS files
//

#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <FS.h>

#include "Webserver.h"
#include "defaults.h"
#include "Sensor.h"
#include "DB.h"


ESP8266WebServer         server(8080);   // Set the HTTP Server port here


Webserver::Webserver() {
  SPIFFS.begin();
  Serial.println("File System Initialized");
}


void Webserver::begin( Config *config, Sensor *sensor, DB *db ) {
  _config = config;  // Keep a reference to the config
  _sensor = sensor;  // Keep a reference to the sensor library
  _db     = db;      // Keep a reference to the db library

  // See if we can find the version of the SPIFFS that we're running
  _spiffs_version = get_spiffs_version();

  _fw_check_interval = FW_CHECK_INTERVAL * 1000;
  _next_fw_check = millis() + _fw_check_interval;

  Serial.println( "[Webserver] HTTP INIT, hostname: " + String( _config->conf.hostname ) + "  port: " + String( _config->conf.http_server_port ) );
  Serial.println( "SPIFFS Version: " + _spiffs_version );
  

//  MDNS.begin( conf.hostname );

  // HTTP callbacks bound to class member functions
  server.on("/",         HTTP_GET,  std::bind(&Webserver::handleWebRequests, this));
  server.on("/config",   HTTP_GET,  std::bind(&Webserver::jsonConfigData, this));
  server.on("/network",  HTTP_POST, std::bind(&Webserver::processNetworkSettings, this));
  server.on("/reset",    HTTP_POST, std::bind(&Webserver::processConfigReset, this));
  server.on("/sensors",  HTTP_GET,  std::bind(&Webserver::jsonSensorData, this));
  server.on("/settings", HTTP_POST, std::bind(&Webserver::processSettings, this));
  server.on("/webupdate", HTTP_POST, std::bind(&Webserver::runWebUpdate, this));
  server.onNotFound(std::bind( &Webserver::handleWebRequests, this));

  // Attach the OTA update service
  _httpUpdater.setup(&server, HTTP_OTA_UPDATE_PATH, HTTP_AUTH_USER, _config->conf.http_pw);
  
  server.begin();
}


void Webserver::loop() {
  // Handle any HTTP Requests
  server.handleClient();

  if (millis() > _next_fw_check) {   // Time to check for updated files
    _next_fw_check = millis() + _fw_check_interval;
    check_for_spiffs_update();
  }
}


void Webserver::check_for_fwupdate() {
  Serial.println("[Webserver] checking for Firmware updates");

  t_httpUpdate_return ret = ESPhttpUpdate.update(UPDATE_URL, INO_VERSION);
//  ret = ESPhttpUpdate.updateSpiffs(UPDATE_URL, INO_VERSION);

  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("[Webserver] HTTP update failed.  Error (%d): %s\n", 
          ESPhttpUpdate.getLastError(),
          ESPhttpUpdate.getLastErrorString().c_str());
      break;

    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("[Webserver] No remote update found.");
      break;

    case HTTP_UPDATE_OK:
      Serial.println("[Webserver] Remote Firmware update succeeded!");
      break;
  }

}


String Webserver::get_spiffs_version() {
  File versionFile = SPIFFS.open("/version.txt", "r");

  if (!versionFile) {
    Serial.println("[Webserver] Failed to open /version.txt, unknown SPIFFS version!");
    return String("");
  }

  if (versionFile.size() > 25) {
    Serial.println("[Webserver] /version.txt is too large!");
    return String("");
  }

  return versionFile.readString();
  
}

void Webserver::check_for_spiffs_update() {
  Serial.println("[Webserver] checking for SPIFFS updates");

  t_httpUpdate_return ret = ESPhttpUpdate.updateSpiffs(UPDATE_URL, String(INO_VERSION) + ":" + _spiffs_version);
//  ret = ESPhttpUpdate.updateSpiffs(UPDATE_URL, INO_VERSION);

  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("[Webserver] HTTP SPIFFS update failed.  Error (%d): %s\n", 
          ESPhttpUpdate.getLastError(),
          ESPhttpUpdate.getLastErrorString().c_str());
      break;

    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("[Webserver] No remote SPIFFS update found.");
      break;

    case HTTP_UPDATE_OK:
      Serial.println("[Webserver] Remote SPIFFS update succeeded!");
      break;
  }

}


void Webserver::handleWebRequests() {
  Serial.println( "[Webserver] handleWebRequests: " +  server.uri() );

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
    message += " NAME:" + server.argName(i) + "\n VALUE:" + server.arg(i) + "\n";
  }
  server.send( 404, "text/plain", message );
  Serial.println( message );
}


// Respond to a web client request
void Webserver::httpReturn(uint16_t httpcode, String mimetype, String content) {
  server.sendHeader("Connection", "close");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(httpcode, mimetype, content);
}


// Handle loading a file from the local file system (SPIFFS)
bool Webserver::loadFromSpiffs( String path ){
  String dataType = "text/plain";
  if (path.endsWith("/")) path += "index.html";
 
  if (path.endsWith(".src")) path = path.substring(0, path.lastIndexOf("."));
  else if (path.endsWith(".html")) dataType = "text/html";
  else if (path.endsWith(".htm")) dataType = "text/html";
  else if (path.endsWith(".css")) dataType = "text/css";
  else if (path.endsWith(".js")) dataType = "application/javascript";
  else if (path.endsWith(".png")) dataType = "image/png";
  else if (path.endsWith(".gif")) dataType = "image/gif";
  else if (path.endsWith(".jpg")) dataType = "image/jpeg";
  else if (path.endsWith(".ico")) dataType = "image/x-icon";
  else if (path.endsWith(".xml")) dataType = "text/xml";
  else if (path.endsWith(".pdf")) dataType = "application/pdf";
  else if (path.endsWith(".zip")) dataType = "application/zip";

  Serial.println("[Webserver] loadFromSpiffs path:" + path + " dataType: " + dataType);
  
  File dataFile = SPIFFS.open(path.c_str(), "r");
  if (server.hasArg("download")) dataType = "application/octet-stream";
  if (server.streamFile(dataFile, dataType) != dataFile.size()) {
    Serial.println("[Webserver] loadFromSpiffs != dataFile.size()" );
  }
 
  dataFile.close();
  return true;
}


// GET /config
// Return a JSON string of the current settings
void Webserver::jsonConfigData() {
  if ( authRequired() ) return;  // Page requires authentication
    
  String jsonstr = _config->JSON( String(WiFi.macAddress()) );
  
  httpReturn(200, "application/json", jsonstr);

}

bool Webserver::authRequired() {
  // TEST_MODE disabled authentication
  if ( TEST_MODE ) return false;

  if ( !server.authenticate( HTTP_AUTH_USER, _config->conf.http_pw ) ) {
    server.requestAuthentication( DIGEST_AUTH, auth_realm, auth_fail_response );
    return true;
  }
  return false;
}



// GET /sensors
// Return Sensor Values in a JSON string
void Webserver::jsonSensorData() {
  String jsonstr = "{\"hum\": " + String(_sensor->get_humidity()) + 
                   ", \"hidx\": " + String(_sensor->get_hindex()) + 
                   ", \"temp\": " + String(_sensor->get_temp()) +
                   ", \"analog\": " + String(_sensor->get_analog()) +
                   ", \"pressure\": " + String(_sensor->get_pressure()) + "}";
  httpReturn(200, "application/json", jsonstr);
}



// POST /reset
void Webserver::processConfigReset() {
  if ( authRequired() ) return;  // Page requires authentication

  // Delay to give the user a chance to pull the plug
  for (int x=5; x>0; x--) {
    Serial.println("[RESET REQUEST] Clearing config in " + String(x) + "...Reset to stop");
    delay(1000);
  }

  // Write defaults to the config
  _config->resetConfig();
  
  httpReturn(200, "application/json", "{\"status\": \"ok\"}");

  // Restart
  ESP.restart();
}


// POST /settings
void Webserver::processSettings() {
  if ( authRequired() ) return;  // Page requires authentication

  // Update config with values from form
  if ( server.hasArg("db_type") )        _config->set( CONFIG_DB_TYPE,         server.arg("db_type") );
  if ( server.hasArg("db_host") )        _config->set( CONFIG_DB_HOST,         server.arg("db_host") );
  if ( server.hasArg("db_port") )        _config->set( CONFIG_DB_PORT,         server.arg("db_port") );
  if ( server.hasArg("db_name") )        _config->set( CONFIG_DB_NAME,         server.arg("db_name") );
  if ( server.hasArg("db_measurement") ) _config->set( CONFIG_DB_MEASUREMENT,  server.arg("db_measurement") );
  if ( server.hasArg("location") )       _config->set( CONFIG_LOCATION,        server.arg("location") );
  if ( server.hasArg("interval") )       _config->set( CONFIG_SAMPLE_INTERVAL, server.arg("interval") );

  if ( server.hasArg("http_pw") )        _config->set( CONFIG_HTTP_PW,         server.arg("http_pw") );
  if ( server.hasArg("t_offset") )       _config->set( CONFIG_T_OFFSET,        server.arg("t_offset") );

  _config->writeConfig();

  // Tell the DB to re-init with new settings
  _db->begin( _config, _sensor );
  // TODO: move this into the db class
//  send_to_db_interval = _config->conf.sample_interval * 1000;
//  next_send_to_db = millis() + send_to_db_interval;

  // Success to the client.
  httpReturn(200, "application/json", "{\"status\": \"ok\"}");

}


// POST /network
void Webserver::processNetworkSettings() {
  if ( authRequired() ) return;  // Page requires authentication

  if (server.args() < 2) {
    httpReturn(400, "text/html", "Missing Data");
    return;
  }

  // Update config with values from form
  _config->set( CONFIG_SSID, server.arg("ssid") );
  _config->set( CONFIG_HOSTNAME, server.arg("hostname") );

  // Only change the wifi password if one was passed
  if ( server.arg("wifi_pw").length() > 0 )
    _config->set( CONFIG_WIFI_PW, server.arg("wifi_pw") );

  // Success to the client.
  httpReturn( 200, "application/json", "{\"status\": \"ok\"}" );

  // Save the running config
  _config->writeConfig();

  // Wait a bit before disconnecting/reconnecting
  delay(2000);

  // Disconnect the wifi and re-run the setup routine
  WiFi.disconnect( true );
  setup();
}

void Webserver::runWebUpdate() {
}


