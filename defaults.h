#define INO_VERSION       "0.3.0"
#define TEST_MODE         1   // turns off authentication for local dev

// DHT22 Globals
#define DHTPWR D3
#define DHTPIN D4
#define DHTTYPE DHT22

// Database Types
#define DB_TYPE_NONE       0
#define DB_TYPE_INFLUXDB   1
#define DB_TYPE_POSTGRESQL 2

#define HTTP_AUTH_USER       "admin"
#define HTTP_OTA_UPDATE_PATH "/firmware"
