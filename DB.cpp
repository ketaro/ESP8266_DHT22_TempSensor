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
uint16_t DB::influxDBSend( String cur_temp, String cur_humidity, String cur_hindex ) {

  // Build the POST
  String influxout;
  influxout = influx_escape( _config->conf.db_measurement ) + ",host=" + influx_escape( _config->conf.hostname ) +
              ",location=" + influx_escape( _config->conf.location ) +
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
