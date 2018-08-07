//
// Webserver.h - Library for wrapping the ESP8266 Web Server
//                 also manages the SPIFFS files
//

#ifndef Webserver_h
#define Webserver_h

#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266HTTPClient.h>

#include "defaults.h"
#include "Config.h"
#include "Sensor.h"
#include "DB.h"

#define FW_CHECK_INTERVAL 60*60*24


//
// Webserver Library Class
class Webserver
{
  public:
    Webserver();

    void begin( Config *config, Sensor *sensor, DB *db );
    void loop();
    bool loadFromSpiffs( String path );

    
  private:
    Config                   *_config;
    Sensor                   *_sensor;
    DB                       *_db;
    ESP8266HTTPUpdateServer  _httpUpdater;  // OTA Update Service
    HTTPClient               _client;

    const char* auth_realm    = "ESP8266 TempSensor";
    String auth_fail_response = "Authentication Failed";

    String _spiffs_version  = "";
    int _fw_check_interval  = FW_CHECK_INTERVAL * 1000;
    int _next_fw_check      = _fw_check_interval;
    unsigned long _last_fw_check = 0;

    bool authRequired();
    void handleWebRequests();
    void httpReturn(uint16_t httpcode, String mimetype, String content);
    void jsonConfigData();
    void jsonSensorData();
    void processConfigReset();
    void processSettings();
    void processNetworkSettings();
    String get_spiffs_version();
    void check_for_fwupdate();
    void check_for_spiffs_update();
    void runWebUpdate();
};

#endif
