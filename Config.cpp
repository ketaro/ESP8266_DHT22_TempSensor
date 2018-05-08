//
// config.cpp - Library for handling storing and retrieving 
//              config settings from EEPROM
//

#include <EEPROM.h>

#include "Arduino.h"
#include "Config.h"
#include "defaults.h"


Config::Config() {
  // init EEPROM, all 512 bytes
  EEPROM.begin( EEPROM_SIZE );

  readConfig();
}


void Config::readConfig() {
  unsigned int current_version;

  Serial.print( "READING CONFIG FROM EEPROM.  VERSION: " );

  // Read the config version number from eeprom
  EEPROM.get( EEPROM_CONFIG_START, current_version );
  Serial.println( current_version, DEC );

  // If config version is not something we can upgrade from,
  // set a default config.
  if ( current_version != 4 ) {
    Serial.println( "Unsupported or no config found in EEPROM.  Resettings to defaults." );
    conf = _defaults;
    writeConfig();
  }

  // Read EEPROMP into config struct
  EEPROM.get( EEPROM_CONFIG_START, conf );
}


//
// Update a value in the running config (does not commit)
// Truncates strings that exceed the max length for any field.
bool Config::set( int key, String value ) {
  Serial.print( "Config::set( " + String(key) + ", " + value + ")" );

  switch( key ) {
    case CONFIG_HOSTNAME:
      strcpy( conf.hostname, value.substring(0, MAX_HOSTNAME).c_str() );
      break;
    case CONFIG_SSID:
      strcpy( conf.ssid, value.substring(0, MAX_SSID).c_str() );
      break;
    case CONFIG_WIFI_PW:
      strcpy( conf.wifi_pw, value.substring(0, MAX_WIFI_PW).c_str() );
      break;

    
    default:
      return false;
  }

  return true;

}


// Save the running config
void Config::writeConfig() {
  Serial.print( "Clearing EEPROM, size: " );
  Serial.println( EEPROM.length() );
  for (int i = 0 ; i < EEPROM.length() ; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  
  Serial.println( "Writing config to EEPROM" );
  EEPROM.put( EEPROM_CONFIG_START, conf );
  EEPROM.commit();

  // Reboot!
  reboot();
}


// Reset the running config
void Config::resetConfig() {
  conf = _defaults;
  writeConfig();
}


// Returns a JSON String of the current config
String Config::JSON(String macaddr) {
  bool wifiPassSaved = false;

  if (String(conf.wifi_pw).length() > 0)
    wifiPassSaved = true;
  
  String jsonstr = "{\"ver\": \"" + String(conf.version) + "\", " 
                   "\"ino_ver\": \"" + INO_VERSION + "\", "
                   "\"mac\": \"" + macaddr + "\", "
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

  return jsonstr;
}

