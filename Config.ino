bool loadSystemConfig() {
  Serial.println("mounting FS...");
  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/" + configJsonFile)) {
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/" + configJsonFile, "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        Serial.println("Content of JSON Config-File: /"+configJsonFile);
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nJSON OK");
          ((json["IP"]).as<String>()).toCharArray(WemosNetConfig.IP, IPSIZE);
          ((json["NETMASK"]).as<String>()).toCharArray(WemosNetConfig.NETMASK, IPSIZE);
          ((json["GW"]).as<String>()).toCharArray(WemosNetConfig.GW, IPSIZE);
          ((json["CCUIP"]).as<String>()).toCharArray(HomeMaticConfig.CCUIP, IPSIZE);

          ((json["VAR_COMMAND"]).as<String>()).toCharArray(HomeMaticConfig.VAR_RFID_command, CCUVARSIZE);
          ((json["VAR_SCHARFSCHALTBAR"]).as<String>()).toCharArray(HomeMaticConfig.VAR_Alarmanlage_scharfschaltbar, CCUVARSIZE);
          ((json["VAR_SCHARF"]).as<String>()).toCharArray(HomeMaticConfig.VAR_Alarmanlage_scharf, CCUVARSIZE);
          ((json["VAR_WIRD_SCHARF"]).as<String>()).toCharArray(HomeMaticConfig.VAR_Alarmanlage_wird_scharf, CCUVARSIZE);
          RFID.Mode = json["RFID_MODE"];          
        } else {
          Serial.println("\nERROR loading config");
        }
      }
      return true;
    } else {
      Serial.println("/" + configJsonFile + " not found.");
      return false;
    }
    SPIFFS.end();
  } else {
    Serial.println("failed to mount FS");
    return false;
  }
}

bool saveSystemConfig() {
  SPIFFS.begin();
  Serial.println("saving config");
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["IP"] = WemosNetConfig.IP;
  json["NETMASK"] = WemosNetConfig.NETMASK;
  json["GW"] = WemosNetConfig.GW;
  json["CCUIP"] = HomeMaticConfig.CCUIP;

  json["VAR_COMMAND"] = HomeMaticConfig.VAR_RFID_command;
  json["VAR_SCHARFSCHALTBAR"] = HomeMaticConfig.VAR_Alarmanlage_scharfschaltbar;
  json["VAR_SCHARF"] = HomeMaticConfig.VAR_Alarmanlage_scharf;
  json["VAR_WIRD_SCHARF"] = HomeMaticConfig.VAR_Alarmanlage_wird_scharf;
  json["RFID_MODE"] = RFID.Mode;

  SPIFFS.remove("/" + configJsonFile);
  File configFile = SPIFFS.open("/" + configJsonFile, "w");
  if (!configFile) {
    Serial.println("failed to open config file for writing");
  }

  json.printTo(Serial);
  json.printTo(configFile);
  configFile.close();

  SPIFFS.end();
}

