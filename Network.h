//
// Network.h - Library for handling network/wifi management
//

#ifndef Network_h
#define Network_h

#include "defaults.h"
#include "Config.h"

// Number of tries to connect to wifi before starting AP
#define WIFI_CONNECT_ATTEMPTS    5     

// How often to test check network connection
#define NETWORK_CHECK_INTERVAL   60*5


//
// Network Library Class
class Network
{
  public:
    Network();
    void begin( Config config );
    void loop();
    bool connected();
    void connect( int attempts );
    void start_ap();
    
    String ssid();
    String ipaddr();
    String macaddr();
    String hostname();


  private:
    Config     *_config;
    const int  _network_check_interval = NETWORK_CHECK_INTERVAL * 1000;
    int        _next_network_check     = _network_check_interval;
    String     _ap_ssid;
    const char *_ap_passwd = DEFAULT_WIFI_PW;
    String     _ipaddr;
    String     _hostname;
    bool       _connected;

};

#endif
