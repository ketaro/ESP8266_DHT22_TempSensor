/*
 *  ESP8266+DHT22 Temperature Sensor
 *  Nick Avgerinos - http://github.com/ketaro
 *  Tyler Eaton - https://github.com/googlegot
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 *  CH340 Chipset Driver (for OSX)
 *  https://github.com/adrianmihalko/ch340g-ch34g-ch34x-mac-os-x-driver
 *
 *  Add Board Manager URL in Arduino IDE
 *  http://arduino.esp8266.com/stable/package_esp8266com_index.json
 *
 *  Plugin for uploading SPIFFS
 *  https://github.com/esp8266/arduino-esp8266fs-plugin
 *
 *  Web Server / SPIFFS inspired by:
 *  https://circuits4you.com/2018/02/03/esp8266-nodemcu-adc-analog-value-on-dial-gauge/
 * 
 *  OTA Update
 *  http://esp8266.github.io/Arduino/versions/2.0.0/doc/ota_updates/ota_updates.html
 *
 *  DoubleResetDetector
 *  https://github.com/datacute/DoubleResetDetector
 */

#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>

#include "defaults.h"
#include "Config.h"
#include "Network.h"
#include "Sensor.h"
#include "DB.h"
#include "Webserver.h"


//
// Helper Classes
Config config;
Network net;
DB db;
Sensor sensor(DHTPIN, DHTTYPE);
Webserver web;


//
// GLOBALS
//
int send_to_db_interval    = 30000;    // 30 seconds
int next_send_to_db        = send_to_db_interval;


//
// A R D U I N O  S E T U P
void setup() {
  delay(1000);
  
  // Start serial
  Serial.begin(115200);
  Serial.println();

  Serial.println("---------------------------");
  Serial.println("HEATSTROKE INO: " + String(INO_VERSION));
  Serial.println("---------------------------");
  Serial.println();
 
  // Initialize Network/WiFi
  net.begin( &config );

  // Start the temperature sensor
  sensor.begin( &config );

  // Initialize the database library
  db.begin( &config, &sensor );
  send_to_db_interval = config.conf.sample_interval * 1000;

  // Initialize File System and Web Server
  web.begin( &config, &sensor, &db );
  delay(500);
}


// Main Arduino Loop
void loop() {
  sensor.loop();
  net.loop();
  web.loop();

  if (millis() > next_send_to_db) {  // Time to send readings to the db
    if ( net.connected() ) {  // Can't send if we're not connected
      db.send();
    } else {
      Serial.println("[NO NETWORK] Cannot send to DB.");
    }
    next_send_to_db = millis() + send_to_db_interval;
  }

  if ( millis() > MAX_RUNTIME ) {
    // If we've been running more than MAX_RUN, just reboot to reset
    // millis() so we don't have to deal with overflow
    Serial.println( "----------------------------------" );
    Serial.println( "MAX RUNTIME REACHED, RESTARTING..." );
    Serial.println( "----------------------------------" );
    ESP.restart();
  }

}


