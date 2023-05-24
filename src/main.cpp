#include <Arduino.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>

uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
esp_now_peer_info_t broadcastPeerInfo;
unsigned long lastHertbeatReceived{0};
unsigned long lastHertbeatSent{0};
String macMdi = "0c:8b:95:d2:9f:d8";
bool peerAlive{false};
esp_now_peer_info_t peerInfo;
uint8_t peerMac[6];
unsigned long waitingPairing{0};

void onDataReceived(const uint8_t *macAddr, const uint8_t *data, int len);
void onDataSent(const uint8_t *macAddr, esp_now_send_status_t status);

void setup() {
  Serial.begin(115200);

  Serial.println("INFO: inicialização do CTI");

  WiFi.mode(WIFI_STA);
  WiFi.begin("LENOVOW 3047", "3rW44(51");
  
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("INFO: aguardando conexão com o WiFi");
    delay(1000);
  }

  Serial.println("INFO: conectado a rede WiFi no canal " + String(WiFi.channel()));

  int channel = WiFi.channel() != 1 ? 1 : 11;

  WiFi.softAP("Esp32", "", channel, 1, 4);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ERRO: erro ao inicializar ESP-NOW");
    Serial.println("INFO: reinicializando");
    ESP.restart();
  }

  esp_now_register_recv_cb(onDataReceived);
  esp_now_register_send_cb(onDataSent);

  memcpy(broadcastPeerInfo.peer_addr, broadcastMac, 6);
  broadcastPeerInfo.channel = WiFi.channel();
  broadcastPeerInfo.encrypt = false;
  broadcastPeerInfo.ifidx = WIFI_IF_AP;
}

void loop() {
  unsigned long now = millis();

  if (!peerAlive && now - waitingPairing > 2e3) {
    waitingPairing = now;
    Serial.println("INFO: aguardando pareamento com MDI");
  }

  if (peerAlive && now - lastHertbeatReceived > 5e3) {
    peerAlive = false;
    Serial.println("INFO: conexão com o MDI perdida");
  }

  if (peerAlive && now - lastHertbeatSent > 2e3) {
    lastHertbeatSent = now;
    String data = "heartbeat";
    esp_now_send((uint8_t*)peerMac, (const uint8_t*)data.c_str(), data.length());
    Serial.println("INFO: enviando heartbeat");
  }
}

void onDataReceived(const uint8_t *macAddr, const uint8_t *data, int len) {
  char macStr[18];

  Serial.print("INFO: pacote recebido de: ");
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x", macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
  Serial.println(macStr);

  char buffer[ESP_NOW_MAX_DATA_LEN + 1];
  int msgLen = min(ESP_NOW_MAX_DATA_LEN, len);

  strncpy(buffer, (const char*) data, msgLen);
  buffer[msgLen] = 0;
  
  if (strcmp(buffer, "registro") == 0) {
    Serial.println("INFO: recebida solicitação de pareamento");
    if (strcmp(macStr, macMdi.c_str()) != 0) {
      Serial.println("INFO: solicitação recusada - mac inválido");
    }
    lastHertbeatReceived = millis();
    memcpy(peerInfo.peer_addr, macAddr, 6);
    memcpy(peerMac, macAddr, 6);
    peerInfo.channel = WiFi.channel();
    peerInfo.ifidx = WIFI_IF_AP;
    if (esp_now_is_peer_exist(macAddr)) {
      esp_now_del_peer(macAddr);
    }
    if (esp_now_add_peer(&peerInfo) == ESP_OK) {
      peerAlive = true;
      Serial.println("INFO: peer adicionado com sucesso");
      Serial.println("INFO: enviando confirmação de registro");
      esp_now_send(NULL, (const uint8_t*)macMdi.c_str(), macMdi.length());
    }
    else {
      Serial.println("ERRO: falha ao adicionar peer");
    }
  }
  else if(strcmp(buffer, "heartbeat") == 0) {
    lastHertbeatReceived = millis();
    Serial.println("INFO: heartbeat do CTI recebido com sucesso");
  }
}

void onDataSent(const uint8_t *macAddr, esp_now_send_status_t status) {
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "INFO: mensagem enviada com sucesso" : "ERRO: falha no envio da mensagem");
}