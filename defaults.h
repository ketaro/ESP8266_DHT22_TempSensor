#define INO_VERSION       "0.5.0"
#define TEST_MODE         0   // turns off authentication for local dev
#define UPDATE_URL        "http://heatstroke.axcella.com:5000/update"

// DHT22 Globals
#define DHTPWR   D3
#define DHTPIN   D4
#define DHTTYPE  DHT22

// Pressure Sensor
#define PRESSURE_PIN A0

// Max Runtime (seconds)
// runtime greater than this interval will cause a reboot
#define MAX_RUNTIME        60*60*2 * 1000

#define HTTP_AUTH_USER       "admin"
#define HTTP_OTA_UPDATE_PATH "/firmware"
