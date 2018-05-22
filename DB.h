/*
 *  DB.h - Library for sending sensor readings to a database (influx)
 *  
 */

#ifndef DB_H
#define DB_H

#include <ESP8266HTTPClient.h>
#include "defaults.h"
#include "Config.h"
#include "Sensor.h"

class DB {
  public:
    DB();
    void     begin( Config &config, Sensor &sensor );
    void     loop();
    void     send();
    uint16_t influxDBSend( float cur_temp, float cur_humidity, float cur_hindex );
    String   urlencode( String text );
    String   influx_escape( String test );

  private:
    String _influx_url;
    HTTPClient _http;
    Config *_config;
    Sensor *_sensor;
  
};

 #endif
