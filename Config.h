//
// config.h - Library for handling storing and retrieving 
//            config settings from EEPROM
//

#ifndef Config_h
#define Config_h

#include "Arduino.h"

#define CONFIG_VERSION           4
#define EEPROM_SIZE              512
#define EEPROM_CONFIG_START      0

#define DEFAULT_HOSTNAME         "esp-dht-1"
#define DEFAULT_HTTP_PORT        8080
#define DEFAULT_SAMPLE_INTERVAL  60
#define DEFAULT_SSID             "SightUnseenFarm"
#define DEFAULT_WIFI_PW          "PASSWORD"

#define DB_TYPE_NONE       0
#define DB_TYPE_INFLUXDB   1
#define DB_TYPE_POSTGRESQL 2

#define CONFIG_HOSTNAME       1
#define CONFIG_LOCATION       2
#define CONFIG_HTTP_PW        3
#define CONFIG_SSID           4
#define CONFIG_WIFI_PW        5
#define CONFIG_DB_HOST        6
#define CONFIG_DB_NAME        7
#define CONFIG_DB_MEASUREMENT 8


#define MAX_HOSTNAME  20
#define MAX_LOCATION  20
#define MAX_HTTP_PW   10
#define MAX_SSID      32
#define MAX_WIFI_PW   32
#define MAX_DB_HOST   64
#define MAX_DB_NAME   20
#define MAX_DB_MEASUREMENT 15

// EEPROM Configuration Structure
struct configuration {
  unsigned int version;

  // System Settings (and +1 for the \0)
  char hostname[ MAX_HOSTNAME+1 ];
  char location[ MAX_LOCATION+1 ];
  unsigned short http_server_port;
  char http_pw[ MAX_HTTP_PW+1 ];

  // Network Settings
  char ssid[ MAX_SSID+1 ];
  char wifi_pw[ MAX_WIFI_PW+1 ];

  // Database Settings
  byte           db_type;             // 0 - none, 1 - influxdb
  char           db_host[ MAX_DB_HOST+1 ];
  unsigned short db_port;
  char           db_name[ MAX_DB_NAME+1 ];
  char           db_measurement[ MAX_DB_MEASUREMENT+1 ];  // Measurement string

  unsigned int sample_interval;

};


class Config
{
  public:
    Config();
    void readConfig();
    bool set( int key, String value );
    void writeConfig();
    void resetConfig();

    void(* reboot) (void) = 0;  // Jump to the beginning (not a full reset, but good enough...)

    String JSON(String macaddr);

    configuration conf;


  private:
    configuration _defaults = { CONFIG_VERSION, DEFAULT_HOSTNAME, "unknown", DEFAULT_HTTP_PORT, "",  
                                DEFAULT_SSID, DEFAULT_WIFI_PW,
                                DB_TYPE_INFLUXDB, "influxdb", 8086, "temp", "ambient", DEFAULT_SAMPLE_INTERVAL  };    

};


#endif
