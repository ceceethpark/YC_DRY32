#include "updateAPI.h"

void UpdateAPI::begin() {
#if ENABLE_API_UPLOAD
  _lastUploadMs = millis();
#endif
}

void UpdateAPI::loop(float tempC) {
#if ENABLE_API_UPLOAD
  unsigned long now = millis();
  if (now - _lastUploadMs < API_UPLOAD_INTERVAL_MS) {
    return;
  }
  _lastUploadMs = now;

  if (isnan(tempC)) {
    // Serial.println("Skip upload: invalid temperature");
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    // Serial.println("Skip upload: WiFi disconnected");
    return;
  }

  bool sent = sendProcessData(tempC);
  // Serial.printf("API upload %s (%.1fC)\n", sent ? "OK" : "FAIL", tempC);
#else
  (void)tempC;
#endif
}

bool UpdateAPI::sendProcessData(float tempC) {
#if ENABLE_API_UPLOAD
  WiFiClient client;
  HTTPClient http;
  if (!http.begin(client, API_ENDPOINT_URL)) {
    // Serial.println("HTTP begin failed");
    return false;
  }

  String boundary = "----DryerBoundary" + String(millis(), HEX);
  http.addHeader(API_HEADER_SECURITY_KEY, API_HEADER_SECURITY_VALUE);
  http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);

  String body;
  auto appendField = [&](const char* key, const String& value) {
    body += "--" + boundary + "\r\n";
    body += "Content-Disposition: form-data; name=\"" + String(key) + "\"\r\n\r\n";
    body += value + "\r\n";
  };

  appendField("product_id", API_PRODUCT_ID);
  appendField("partner_id", API_PARTNER_ID);
  appendField("machine_id", API_MACHINE_ID);
  appendField("record_id", String(API_RECORD_ID) + "_" + String((uint32_t)(millis() / 1000)));
  appendField("measure_value", String(tempC, 1));
  appendField("departure_yn", API_DEPARTURE_YN);
  body += "--" + boundary + "--\r\n";

  int status = http.POST(body);
  if (status <= 0) {
    // Serial.printf("HTTP POST failed: %d\n", status);
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  // Serial.printf("API response %d: %s\n", status, payload.c_str());
  return status >= 200 && status < 300;
#else
  (void)tempC;
  return false;
#endif
}
