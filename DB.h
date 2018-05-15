/*
 *  DB.h - Library for sending sensor readings to a database (influx)
 *  
 */

#ifndef DB_H
#define DB_H

#include <ESP8266HTTPClient.h>
#include "defaults.h"
#include "Config.h"


class DB {
  public:
    DB();
    void     init( Config &config );
    void     send( String cur_temp, String cur_humidity, String cur_hindex );
    uint16_t influxDBSend( String cur_temp, String cur_humidity, String cur_hindex );
    String   urlencode( String text );

  private:
    String _influx_url;
    HTTPClient _http;
    Config *_config;
  
};

 #endif
