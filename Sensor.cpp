//
// Sensor.cpp - Library for handling readings from the temp sensor.
//

#include <DHT.h>

#include "Sensor.h"
#include "defaults.h"


//Sensor::Sensor(uint8_t pin, uint8_t type) {
//  Serial.println("[Sensor] DHT(" + String(pin) + ", " + String(type) + ")");
//  _dht = DHT(pin, type);
//};


void Sensor::begin( Config *config ) {
  // Keep a reference to the config
  _config = config;
  _poll_sensor_interval = SENSOR_POLL_INTERVAL * 1000;
  _next_sensor_poll = millis() + _poll_sensor_interval;

  sensor_on();
  _dht.begin();
}


void Sensor::loop() {
  if (millis() > _next_sensor_poll) {   // Time to read the sensors
    read_sensor();
    _next_sensor_poll = millis() + _poll_sensor_interval;
  }
}


// Main Sensor Turn On
void Sensor::sensor_on() {
  pinMode(DHTPWR, OUTPUT);
  digitalWrite(DHTPWR, 1);
}


// Try to reset the DHT sensor.  This only works if your
// sensor is wired to use the DHTPWR pin
void Sensor::reset_sensor() {
  Serial.println("Resetting Sensor...");

  digitalWrite(DHTPWR, 0);
  delay(500);
  digitalWrite(DHTPWR, 1);

  // Give the sensor a cooling off by skipping the next interval
  _next_sensor_poll = millis() + _poll_sensor_interval*2;
//  delay(SENSOR_DELAY_AFTER_RESET);
  
  _dht.begin();
}

// Reading Getters
float Sensor::get_temp()     { return _cur_temp; }
float Sensor::get_humidity() { return _cur_humidity; }
float Sensor::get_hindex()   { return _cur_hindex; }
float Sensor::get_analog()   { return _cur_analog; }
float Sensor::get_pressure() {
  if (_cur_analog)
    return map(_cur_analog, 0, 1023, 0, 200); 
  return NAN;
}


// Attempt to read sensor values from the DHT22
void Sensor::read_sensor() {
    Serial.println("[Sensor] read_sensor");

    // Subtract the temperature offset due to heating from the MCU
    float temp     = _dht.readTemperature(true) - _config->conf.t_offset;
    float humidity = _dht.readHumidity();

    if (isnan(temp) || isnan(humidity)) {
      // Successfully failed to get a readout from the DHT22
      Serial.println("[Sensor] Failed to get reading from DHT22");

      // If we haven't got a reading in SENSOR_RESET_INTERVAL, reset the sensor
      if (millis() - _last_sensor_read > SENSOR_RESET_INTERVAL * 1000) {
        reset_sensor();  // may the odds be ever in your favor.
        _last_sensor_read = millis();   // not really, but set the counters based off now

        // Clear out current readings
        _cur_temp     = NAN;
        _cur_humidity = NAN;
        _cur_hindex   = NAN;
      }

    } else {
      // Successfully got a readout
      _last_sensor_read = millis();
   
      _cur_temp     = temp;
      _cur_humidity = humidity;
      _cur_hindex   = _dht.computeHeatIndex(temp, humidity);

      // Write out to serial
      Serial.println("[Sensor] Temp: " + String(_cur_temp, 2) + "F   " 
                    "Humidity: " + String(_cur_humidity, 2) + "%   "
                    "Heat Index: " + String(_cur_hindex, 2));
    }

    // Calculate Pressure
    read_analog();
}


// Sample the analog sensor
void Sensor::read_analog() {
  int sum = 0;
  for(int i=0; i < 32; i++) {
    delayMicroseconds(100);
    sum += analogRead(PRESSURE_PIN);
  }
  _cur_analog = float(sum) / 32.0;
  Serial.println("[Sensor] Analog Sensor: " + String(_cur_analog));
}


