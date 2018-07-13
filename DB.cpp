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
void DB::begin( Config *config, Sensor *sensor ) {
  // Keep a reference to the config & sensor
  _config = config;
  _sensor = sensor;
  
  // Create the URL we'll be sending influx data to
  _influx_url = "";
  if (_config->conf.db_type == DB_TYPE_INFLUXDB) {
    _influx_url = "http://" + 
                  String(_config->conf.db_host) + ":" + String(_config->conf.db_port) +
                  "/write?db=" + String(_config->conf.db_name);

    // Start the HTTP Client connection
    _http.begin( _influx_url );
  }


  // Print the influx server info
  Serial.println( "[DB] Influx Server: " + _influx_url );

}

void DB::loop() {
  
}


// URL encode a string
String DB::urlencode( String text ) {
  String encoded = "";

  // Scan through each character, if it's special, encode it.
  for (int i=0; i < text.length(); i++) {
    char c = text.charAt(i);

    if ( c == ' ' )         // Handle spaces
      encoded += '+';
    else if ( isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' )  // Unreserved characters..
      encoded += c;
    else {
      // Other characters need to get converted to their hex codes
      char enchar[5] = "";
      sprintf(enchar, "%%%X", c);
      encoded += enchar;
    }

  }

  return encoded;
}

//
// Escape a string based on the InfluxDB Line Protocol
String DB::influx_escape( String text ) {
  String encoded = "";

  // Scan through each character, if it's special, escape it
  for (int i=0; i < text.length(); i++) {
    char c = text.charAt(i);

    if ( c == ',' || c == '=' || c == ' ' || c == '"' ) {
      encoded += "\\";
      encoded += c;
    } else
      encoded += c;
  }

  return encoded;
}


// Send readings to database
uint16_t DB::influxDBSend( float temp, float humidity, float hindex ) {
  
  // Build the POST
  String influxout;
  influxout = influx_escape( _config->conf.db_measurement ) + ",host=" + influx_escape( _config->conf.hostname ) +
              ",location=" + influx_escape( _config->conf.location ) +
              " temperature=" + String(temp, 2) +
              ",humidity=" + String(humidity, 2) +
              ",heat_index=" + String(hindex, 2);

  Serial.println( "[InfluxDB] " + _influx_url );
  Serial.println( "[InfluxDB] " + String(influxout) );

  // POST the POST and return the result
  return _http.POST(influxout);
  
}


// Send sensor readings to database
// (if the network is available!)
void DB::send() {
  float temp     = _sensor->get_temp();
  float humidity = _sensor->get_humidity();
  float hindex   = _sensor->get_hindex();

  if (isnan(temp) || isnan(humidity) || isnan(hindex)) {
     Serial.println( "[InfluxDB] No Sensor Readings Available to Send!" );
     return;
  }

  if (_config->conf.db_type == DB_TYPE_INFLUXDB) {
    uint16_t httpCode = influxDBSend( temp, humidity, hindex );

    // Parse the return
    // HTTP Code 204 is successful for influxDB.
    if (httpCode != HTTP_CODE_OK && httpCode != 204) {
      Serial.println( "[DB] INFLUX HTTP ERROR: " + String(httpCode) );
    }

  } else if (_config->conf.db_type == DB_TYPE_HTTP) {
    // TODO HTTP CALL
    Serial.println( "[DB] IF AN HTTP CALL WAS CONFIGURED IT WOULD HAPPEN HERE." );
  }

}
