#define INO_VERSION       "0.4.0"
#define TEST_MODE         0   // turns off authentication for local dev

// DHT22 Globals
#define DHTPWR   D3
#define DHTPIN   D4
#define DHTTYPE  DHT22

// Max Runtime (seconds)
// runtime greater than this interval will cause a reboot
#define MAX_RUNTIME        60*60*2 * 1000

#define HTTP_AUTH_USER       "admin"
#define HTTP_OTA_UPDATE_PATH "/firmware"
