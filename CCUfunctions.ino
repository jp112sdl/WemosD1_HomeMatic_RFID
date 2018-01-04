HTTPClient http;

bool setStateCCU(String id, String value) {
  if (WiFi.status() == WL_CONNECTED) {
    if (String(HomeMaticConfig.CCUIP) != "0.0.0.0") {
      http.setTimeout(HTTPTIMEOUT);
      String url = "http://" + String(HomeMaticConfig.CCUIP) + ":8181/rfid.exe?ret=dom.GetObject(%22" + id + "%22).State(" + value + ")";
      Serial.print("setStateCCU url: " + url + " -> ");
      http.begin(url);
      int httpCode = http.GET();
      String payload = "";

      if (httpCode > 0) {
        Serial.print("HTTP " + id + " success -> ");
        payload = http.getString();
      }
      if (httpCode != 200) {
        Serial.println("HTTP " + id + " fail");
        return false;
      }
      http.end();

      payload = payload.substring(payload.indexOf("<ret>"));
      payload = payload.substring(5, payload.indexOf("</ret>"));
      Serial.println("result: " + payload);

      return (payload != "null");
    } else return true;
  } else ESP.restart();
}

String getStateCCU(String id, String type) {
  if (WiFi.status() == WL_CONNECTED) {
    if (String(HomeMaticConfig.CCUIP) != "0.0.0.0") {
      http.setTimeout(HTTPTIMEOUT);
      String url = "http://" + String(HomeMaticConfig.CCUIP) + ":8181/rfid.exe?ret=dom.GetObject(%22" + id + "%22)." + type + "()";
      Serial.print("getStateCCU url: " + url + " -> ");
      http.begin(url);
      int httpCode = http.GET();
      String payload = "error";
      if (httpCode > 0) {
        Serial.print("HTTP " + id + " success -> ");
        payload = http.getString();
      }
      if (httpCode != 200) {
        Serial.println("HTTP " + id + " fail");
        return "";
      }
      http.end();

      payload = payload.substring(payload.indexOf("<ret>"));
      payload = payload.substring(5, payload.indexOf("</ret>"));
      Serial.println("result: " + payload);
      return payload;
    } else return "";
  } else ESP.restart();
}

