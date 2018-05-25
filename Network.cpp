//
// Network.cpp - Library for handling network/wifi management
//

#include <ESP8266WiFi.h>

#include "Network.h"
#include "Config.h"

Network::Network() {
  // Set the SSID we'll use in AP mode
  //_ap_ssid   = "ESP-" + String(ESP.getChipId());
  _ap_ssid   = "ESP-" + String(WiFi.macAddress()).substring(9);
  _ap_ssid.replace(":", "");
  _ap_passwd = DEFAULT_WIFI_PW;

  _next_network_check = _network_check_interval;
}


void Network::begin( Config *config ) {
  // Keep a reference to the config
  _config = config;
  
  // Try WIFI_CONNECT_ATTEMPTS times to get on the network
  connect( WIFI_CONNECT_ATTEMPTS );

  // If after trying to connect we're still not connected, 
  // start in Access Point mode.
  if ( !connected() ) {
    Serial.println( "[Network] WiFi Connection failed, Starting AP..." );

    // Start the access point
    start_ap();
  }
}

void Network::loop() {
  if ( millis() > _next_network_check ) {
    if ( !connected() )
      connect( WIFI_CONNECT_ATTEMPTS );
    else
      Serial.printf( "[Network] Signal Strength: %d dBm\n", WiFi.RSSI() );

    if ( !connected() )
      start_ap();

    _next_network_check = millis() + _network_check_interval;
  }
  
}

// Returns true if the wifi is connected
bool Network::connected() {
  if ( WiFi.status() != WL_CONNECTED )
    return false;

  return true;
}


String Network::ssid() { return WiFi.SSID(); }
String Network::ipaddr() { return _ipaddr; }
String Network::macaddr() { WiFi.macAddress(); }
String Network::hostname() { return WiFi.hostname(); }


// Attempt to connect to the SSID info in our config.
void Network::connect( int attempts ) {
  WiFi.mode( WIFI_STA );
  WiFi.hostname( _config->conf.hostname );
  WiFi.begin( _config->conf.ssid, _config->conf.wifi_pw );

  // Try *attempts* times to get on the network
  Serial.println( "[Network] Mac Address: " + WiFi.macAddress() );
  Serial.print( "[Network] Connecting to WiFi (" + String( _config->conf.ssid ) + ")" );
  for ( int x=0; x < attempts; x++ ) {
    if ( connected() )
      break;
    
    Serial.print(".");
    delay(5000);
  }
  Serial.println();

  if ( WiFi.status() == WL_CONNECTED ) {
    _ipaddr = WiFi.localIP().toString();
    Serial.println( "[Network] Connected to: " + ssid() + "  IP: " + _ipaddr );
  }

}


//
// Start Access Point
void Network::start_ap() {
  WiFi.mode( WIFI_AP );
  WiFi.softAP( _ap_ssid.c_str(), _ap_passwd );
  
  _ipaddr = WiFi.softAPIP().toString();
  Serial.println("AP SSID:" + _ap_ssid + "  Web config IP: http://" + _ipaddr + ":8080");
}

