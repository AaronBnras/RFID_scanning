#include <ESP8266WiFi.h>
#include <SPI.h>
#include <MFRC522.h>
#include <FirebaseESP8266.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#define WIFI_SSID "AaronB"
#define WIFI_PASSWORD "yngezy1213"

#define FIREBASE_HOST "https://voting-system-2241a-default-rtdb.firebaseio.com/"
#define FIREBASE_AUTH "secret data key 🔐"

#define SS_PIN D2
#define RST_PIN D1

MFRC522 rfid(SS_PIN, RST_PIN);
FirebaseData firebaseData;
FirebaseAuth auth;
FirebaseConfig config;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

const String AUTHORIZED_UIDS[] = {
  "F5CC7901",
  "D3EA0E29"
};
const int NUM_AUTHORIZED_UIDS = sizeof(AUTHORIZED_UIDS) / sizeof(AUTHORIZED_UIDS[0]);

void setup() {
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());

  config.database_url = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;

  Serial.println("Connecting to Firebase...");
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  timeClient.begin();
  timeClient.setTimeOffset(0); // Adjust this based on your timezone

  Serial.println("RFID Reader Initialized");
}

void loop() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial())
    return;

  String scannedUID = getUID(rfid.uid.uidByte, rfid.uid.size);
  bool authorized = isAuthorized(scannedUID);
  
  Serial.print("Scanned UID: ");
  Serial.println(scannedUID);
  
  if (authorized) {
    Serial.println("Access Authorized!");
  } else {
    Serial.println("Access denied!");
  }

  sendToFirebase(scannedUID, authorized);

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  delay(1000);
}

String getUID(byte *buffer, byte bufferSize) {
  String uid = "";
  for (byte i = 0; i < bufferSize; i++) {
    uid += (buffer[i] < 0x10 ? "0" : "");
    uid += String(buffer[i], HEX);
  }
  uid.toUpperCase();
  return uid;
}

bool isAuthorized(String uid) {
  for (int i = 0; i < NUM_AUTHORIZED_UIDS; i++) {
    if (uid == AUTHORIZED_UIDS[i]) {
      return true;
    }
  }
  return false;
}

void sendToFirebase(String uid, bool authorized) {
  if (!Firebase.ready()) {
    Serial.println("Firebase not ready");
    return;
  }

  timeClient.update();
  String timestamp = String(timeClient.getEpochTime());
  
  String path = "/rfid_scans";

  FirebaseJson json;
  json.add("ID", uid);
  json.add("status", authorized ? "granted" : "denied");
  json.add("time", timestamp);

  if (Firebase.pushJSON(firebaseData, path, json)) {
    Serial.println("Data sent to Firebase successfully");
  } else {
    Serial.println("Failed to send data to Firebase");
    Serial.println("Reason: " + firebaseData.errorReason());
  }
}
