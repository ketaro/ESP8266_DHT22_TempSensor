//
// Webserver.h - Library for wrapping the ESP8266 Web Server
//                 also manages the SPIFFS files
//

#ifndef Webserver_h
#define Webserver_h

#include <ESP8266HTTPUpdateServer.h>

#include "defaults.h"
#include "Config.h"
#include "Sensor.h"
#include "DB.h"

#define SPIFFS_CHECK_INTERVAL 60*5


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
    const char* auth_realm    = "ESP8266 TempSensor";
    String auth_fail_response = "Authentication Failed";

    int _spiffs_check_interval  = SPIFFS_CHECK_INTERVAL * 1000;
    int _next_spiffs_check      = _spiffs_check_interval;
    unsigned long _last_spifss_check = 0;

    bool authRequired();
    void handleWebRequests();
    void httpReturn(uint16_t httpcode, String mimetype, String content);
    void jsonConfigData();
    void jsonSensorData();
    void processConfigReset();
    void processSettings();
    void processNetworkSettings();
    void spiffs_check_for_update();
};

#endif
