//
// Sensor.h - Library for handling readings from the temp sensor.
//

#ifndef Sensor_h
#define Sensor_h

#define DHT_DEBUG
#include <DHT.h>
#include "defaults.h"
#include "Config.h"

#define SENSOR_POLL_INTERVAL     10    // Seconds
#define SENSOR_RESET_INTERVAL    60    // Reset the sensor if it's been at least this many
                                       // seconds since we last got a successful reading

//
// Sensor Library Class
class Sensor
{
  public:
    Sensor(uint8_t pin, uint8_t type): _dht(pin, type) { };

    void begin( Config config );
    void loop();

    void sensor_on();
    void read_sensor();
    void reset_sensor();
    
    float get_temp();
    float get_humidity();
    float get_hindex();
    
  private:
    Config     *_config;
    DHT        _dht;
//    DHT        _dht(DHTPIN, DHTTYPE);

    int _poll_sensor_interval   = SENSOR_POLL_INTERVAL * 1000;    // Read the sensors every 10 seconds
    int _next_sensor_poll       = _poll_sensor_interval;

    unsigned long _last_sensor_read = 0;
    float _cur_temp       = NAN;
    float _cur_humidity   = NAN;
    float _cur_hindex     = NAN;

};

#endif
