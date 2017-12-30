#include <ESP8266WiFi.h>
#include <SPI.h>
#include <ESP8266HTTPClient.h>
#include <MFRC522.h>
#include <ArduinoOTA.h>
#include <WiFiUdp.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>

#define SS_PIN              D8
#define RST_PIN             D3
#define BUZZER_PIN          D1
#define SWITCHPIN           D2
#define LEDPIN              D4

#define RFIDDETECTLOOPS     60

#define IPSIZE              16
#define CCUVARSIZE          255

#define HTTPTIMEOUT         3000
#define CONFIGPORTALTIMEOUT 180
#define UDPPORT             6673

enum rfid_mode_e { RM_MFRC522, RM_RDM6300 };

struct hmconfig_t {
  char CCUIP[IPSIZE]                                = "";
  char VAR_RFID_command[CCUVARSIZE]                 = "";
  char VAR_Alarmanlage_scharf[CCUVARSIZE]           = "";
  char VAR_Alarmanlage_wird_scharf[CCUVARSIZE]      = "";
  char VAR_Alarmanlage_scharfschaltbar[CCUVARSIZE]  = "";
} HomeMaticConfig;

struct rfid_t {
  int detectCount   = 0;
  int noDetectCount = 0;
  String chipId     = "";
  byte Mode = RM_MFRC522;
} RFID;

struct systemzustand_t {
  bool Scharf          = false;
  bool Scharfschaltbar = false;
  bool AlarmAusgeloest = false;
} Systemzustand;

//WifiManager - don't touch
bool shouldSaveConfig          = false;
const String configJsonFile = "config.json";
#define wifiManagerDebugOutput  true
bool startWifiManager = false;
struct wemosnetconfig_t {
  char IP[IPSIZE]      = "0.0.0.0";
  char NETMASK[IPSIZE] = "0.0.0.0";
  char GW[IPSIZE]      = "0.0.0.0";
} WemosNetConfig;

struct udp_t {
  WiFiUDP UDP;
  char incomingPacket[255];
} UDPClient;

unsigned long oldStandbyBlinkMillis = 0;
unsigned long oldBeepMillis = 0;
unsigned long lastRDM6300ChipIdMillis = 0;

String result1 = "";
String result2 = "";

MFRC522 mfrc522(SS_PIN, RST_PIN);

void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  Serial.begin(9600);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  pinMode(SWITCHPIN, INPUT_PULLUP);

  Serial.println("\nConfig-Modus aktivieren?");
  for (int i = 0; i < 20; i++) {
    if (digitalRead(SWITCHPIN) == LOW) {
      startWifiManager = true;
      break;
    }
    digitalWrite(LEDPIN, HIGH);
    delay(100);
    digitalWrite(LEDPIN, LOW);
    delay(100);
  }

  loadSystemConfig();
  doWifiConnect();
  startOTAhandling();
  UDPClient.UDP.begin(UDPPORT);

  SPI.begin();
  mfrc522.PCD_Init();
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);

  if (getStateCCU(String(HomeMaticConfig.VAR_Alarmanlage_scharf), "State") == "false") {
    Systemzustand.Scharf = false;
    digitalWrite(LED_BUILTIN, HIGH);
  } else {
    Systemzustand.Scharf = true;
    digitalWrite(LED_BUILTIN, LOW);
  }
  Serial.println("Ready!");
}

void loop() {
  ArduinoOTA.handle();
  //Millisekundenüberlauf nach 49... Tagen abfangen
  if (oldStandbyBlinkMillis > millis())
    oldStandbyBlinkMillis = millis();
  if (oldBeepMillis > millis())
    oldBeepMillis = millis();

  if (RFID.Mode == RM_MFRC522) {
    result1 = handleMFRC522();
    result2 = handleMFRC522();
  }

  if (RFID.Mode == RM_RDM6300) {
    delay(20);

    String rdm_result = handleRDM6300();
    if (rdm_result.length() > 10) {
      result1 = rdm_result;
      Serial.println("RDM6300: " + result1);
      delay(20);
      lastRDM6300ChipIdMillis = millis();
    }

    if (millis() - lastRDM6300ChipIdMillis > 200) {
      if (lastRDM6300ChipIdMillis > 0)
        Serial.println("lastRDM6300ChipIdMillis > 200");
      lastRDM6300ChipIdMillis = 0;
      result1 = "";
    }
  }

  if (result1 != result2) {
    RFID.detectCount++;
    if (RFID.detectCount == 1) {
      Serial.println("BEEP");
      beep(70);
    }
    RFID.chipId = result1 + result2;
    //Wenn Chip LANGE gehalten wird
    if (RFID.detectCount == RFIDDETECTLOOPS) {
      Serial.println("\nLONG HOLD (" + String(RFID.detectCount) + ")  " + RFID.chipId);
      //prüfen, ob sich Alarm scharf schalten lässt
      if (getStateCCU(String(HomeMaticConfig.VAR_Alarmanlage_scharfschaltbar), "State") == "true") {
        Serial.println("Scharfschaltbar!");
        Systemzustand.Scharfschaltbar = true;
        //Sende Befehl zum Scharfschalten
        setStateCCU(HomeMaticConfig.VAR_RFID_command, "\"1;" + RFID.chipId + "\"");
      } else {
        //Wenn nicht scharfschaltbar, dann nur Piepton ausgeben
        Systemzustand.Scharfschaltbar = false;
        Serial.println("Nicht scharfschaltbar!");
        for (int i = 0; i < 10; i++) {
          beep(50);
          delay(10);
        }
      }
      RFID.chipId = "";
      if (Systemzustand.Scharfschaltbar == true) {
        delay(150); // <- Wartepause wegen lahmer CCU2, bei RaspberryMatic nicht erforderlich
        //anschließend prüfen, ob Anlage scharf geschaltet wird
        if (getStateCCU(String(HomeMaticConfig.VAR_Alarmanlage_wird_scharf), "State") == "true") {
          digitalWrite(LED_BUILTIN, LOW);
          Systemzustand.Scharf = true;
          beep(1500);
        }
        else {
          //Chip nicht zugelassen
          Serial.println("Chip nicht zugelassen!");
          for (int i = 0; i < 2; i++) {
            beep(50);
            delay(10);
          }
        }
      }
    }
  } else {
    RFID.noDetectCount++;
    if (RFID.noDetectCount == 4) {
      //Wenn Chip KURZ gehalten wird
      if (RFID.detectCount > 0 && RFID.detectCount < (RFIDDETECTLOOPS) && RFID.chipId != "") {
        Serial.println("\nSHORT HOLD (" + String(RFID.detectCount) + ") " + RFID.chipId);
        //Sende Befehl für unscharf
        setStateCCU(HomeMaticConfig.VAR_RFID_command, "\"0;" + RFID.chipId + "\"");
        RFID.chipId = "";
        delay(150); // <- Wartepause wegen lahmer CCU2, bei RaspberryMatic nicht erforderlich
        //Prüfe, ob tatsächlich unscharf
        if (getStateCCU(HomeMaticConfig.VAR_Alarmanlage_scharf, "State") == "false") {
          digitalWrite(LED_BUILTIN, HIGH);
          Systemzustand.Scharf = false;
          Systemzustand.AlarmAusgeloest = false;
          beep(150);
        }
        else {
          //Sonst ist da was faul
          for (int i = 0; i < 2; i++) {
            beep(50);
            delay(10);
          }
        }
      }
      RFID.noDetectCount = 0;
      RFID.detectCount = 0;
    }
  }

  String udpMessage = handleUDP();
  if (udpMessage != "") {
    bool oldScharf = Systemzustand.Scharf;
    Serial.println("UDP Info erhalten: " + udpMessage);
    if (udpMessage == "0") Systemzustand.Scharf = false;
    if (udpMessage == "1") Systemzustand.Scharf = true;
    digitalWrite(LED_BUILTIN, (udpMessage == "0"));

    if (oldScharf != Systemzustand.Scharf) {
      beep(800);
      delay(50);
      beep(100);
      delay(50);
      beep(100);
    }
    Systemzustand.AlarmAusgeloest = (udpMessage == "2");
  }
  standByBlink();
  if (Systemzustand.AlarmAusgeloest) beep(50, 250);
}

String handleMFRC522() {
  if (!mfrc522.PICC_IsNewCardPresent()) return "";
  if (!mfrc522.PICC_ReadCardSerial()) return "";
  return printHex(mfrc522.uid.uidByte, mfrc522.uid.size);
}

String handleRDM6300() {
  if (Serial.available()) {
    String rdm6300_serial = "";
    while (Serial.available()) {
      char inChr = Serial.read();
      switch (inChr) {
        case 0x02:
          rdm6300_serial = "";
        case 0x03:
          break;
        default:
          rdm6300_serial += inChr;
          break;
      }
    }
    return rdm6300_serial;
  }
  return "";
}

String printHex(byte *buffer, byte bufferSize) {
  String id = "";
  for (byte i = 0; i < bufferSize; i++) {
    id += buffer[i] < 0x10 ? "0" : "";
    id += String(buffer[i], HEX);
  }
  id.toUpperCase();
  return id;
}

void beep(int msec) {
  beep(msec, 0);
}
void beep(int msec, int interval) {
  if ((millis() - oldBeepMillis) > interval) {
    oldBeepMillis = millis();
    digitalWrite(BUZZER_PIN, HIGH);
    delay(msec);
    digitalWrite(BUZZER_PIN, LOW);
  }
}

void standByBlink() {
  if ((millis() - oldStandbyBlinkMillis > 4000) &&  !Systemzustand.Scharf) {
    oldStandbyBlinkMillis = millis();
    digitalWrite(LED_BUILTIN, LOW);
    delay(80);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(70);
  }
}
