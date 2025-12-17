#include <WiFi.h>
#include <ArduinoOTA.h>

const char* ssid = "cecee";
const char* pass = "898900898900";
const char* ota_pass = "1234";

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("IP: "); Serial.println(WiFi.localIP());
  ArduinoOTA.setPassword(ota_pass);
  ArduinoOTA.onStart([](){ Serial.println("OTA Start"); });
  ArduinoOTA.onEnd([](){ Serial.println("OTA End"); });
  ArduinoOTA.onProgress([](unsigned int p, unsigned int t){ Serial.printf("OTA Progress: %u%%\n", (p*100)/t); });
  ArduinoOTA.onError([](ota_error_t e){ Serial.printf("OTA Error[%u]\n", e); });
  ArduinoOTA.begin();
  Serial.println("OTA Ready");
}

void loop() {
  ArduinoOTA.handle();
  delay(1);
}
