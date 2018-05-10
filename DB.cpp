/*
 *  DB.cpp - Library for sending sensor readings to a database (influx)
 *  
 */

#include <ESP8266HTTPClient.h>

#include "DB.h"
#include "Config.h"


// Init DB Class
DB::DB() {
  _influx_url = String("");
}


// Setup database based on current config values
void DB::init( Config &config ) {
  // Keep a refernce to the config
  _config = &config;
  
  // Create the URL we'll be sending influx data to
  _influx_url = "http://" + 
                String(_config->conf.db_host) + ":" + String(_config->conf.db_port) +
                "/write?db=" + String(_config->conf.db_name);

  // Start the HTTP Client connection
  _http.begin( _influx_url );

  // Print the influx server info
  Serial.println( "[DB] Influx Server: " + _influx_url );

}


// Send readings to database
uint16_t DB::influxDBSend( String cur_temp, String cur_humidity, String cur_hindex ) {

  // Build the POST
  String influxout;
  influxout = "ambient,host=" + String(_config->conf.hostname) + 
              ",location=" + String(_config->conf.location) + 
              " temperature=" + cur_temp + 
              ",humidity=" + cur_humidity + 
              ",heat_index=" + cur_hindex;

  Serial.println( "[InfluxDB] " + _influx_url );
  Serial.println( "[InfluxDB] " + String(influxout) );
              
  // POST the POST and return the result
  return _http.POST(influxout);
  
}



// Send sensor readings to database
// (if the network is available!)
void DB::send( String cur_temp, String cur_humidity, String cur_hindex ) {

  // TODO: Eventually this can be a DB type check
  // when we support more thing that just InfluxDB  
  uint16_t httpCode = influxDBSend( cur_temp, cur_humidity, cur_hindex );

  // Parse the return
  // HTTP Code 204 is successful for influxDB.
  if (httpCode != HTTP_CODE_OK && httpCode != 204) {
    Serial.println( "[DB] INFLUX HTTP ERROR: " + String(httpCode) );
  }

}
