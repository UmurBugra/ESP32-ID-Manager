#include <SPI.h>
#include <MFRC522.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <WiFi.h>
#include "webpage.h" 

#define RST_PIN 22
#define SS_PIN 5
#define BASLANGIC_BLOK 4

TaskHandle_t rfidTaskHandle = NULL;
TaskHandle_t webServerTaskHandle = NULL;
TaskHandle_t dataProcessTaskHandle = NULL;

QueueHandle_t tagQueue = NULL; 

SemaphoreHandle_t dataAccessMutex = NULL;
SemaphoreHandle_t cardOpMutex = NULL;  

MFRC522 mfrc522(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;
MFRC522::StatusCode status;

const char* ssid = "ESP32-Access-Point";
const char* password = "nwakaeme";    // Anthony Nwakaeme respect
WiFiServer server(80);

// Veri yapıları
struct TagData {
  String content;
  unsigned long timestamp;
};

struct WebRequest {
  bool isVeriKontrol;
  String currentTag;
};

struct CardOperation {
  bool isUpdateRequested;
  String newValue;
  bool isProcessing;
  String status;
};

CardOperation cardOp = {false, "", false, ""};
volatile bool writeRequested = false;


bool handleKimlikBelirleme(MFRC522::StatusCode status, byte buffer[], byte size) {
 
  String currentContent = "";
  for (byte i = 0; i < 16 && buffer[i] != 0; i++) {
    currentContent += (char)buffer[i];
  }
  
  if (currentContent.length() > 0) {
    if(xSemaphoreTake(cardOpMutex, portMAX_DELAY) == pdTRUE) {
      cardOp.status = "Kimlikli Kart! İşlem iptal edildi.";
      writeRequested = false;
      cardOp.isProcessing = false;
      xSemaphoreGive(cardOpMutex);
    }
    Serial.println("Hata: Bu kart zaten kimlikli! Mevcut içerik: " + currentContent);
    Serial.println("İşlem sonlandırılıyor");
    return false;
  }
  
  Serial.println("Yeni kimlik yazılıyor: " + cardOp.newValue);
  byte writeBuffer[16] = {0};
  cardOp.newValue.getBytes(writeBuffer, 16);
  status = mfrc522.MIFARE_Write(BASLANGIC_BLOK, writeBuffer, 16);
  
  if (status != MFRC522::STATUS_OK) {
    if(xSemaphoreTake(cardOpMutex, portMAX_DELAY) == pdTRUE) {
      cardOp.status = "Yazma hatası!";
      cardOp.isProcessing = false;
      xSemaphoreGive(cardOpMutex);
    }
    Serial.println("Hata: Yeni kimlik yazılamadı!");
    return false;
  }
  
  if(xSemaphoreTake(cardOpMutex, portMAX_DELAY) == pdTRUE) {
    cardOp.status = "Kimlik belirlendi!";
    writeRequested = false;
    cardOp.isProcessing = false;
    xSemaphoreGive(cardOpMutex);
  }
  Serial.println("Başarılı: Kimlik belirlendi!");
  return true;
}

void rfidTask(void* parameter) {
  String lastReadTag = "";
  unsigned long lastReadTime = 0;
  const TickType_t xDelay = pdMS_TO_TICKS(50);
  const unsigned long READ_DELAY = 2000;

 
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }

  for (;;) {
    unsigned long currentTime = millis();
    
    if (currentTime - lastReadTime < READ_DELAY) {
      vTaskDelay(xDelay);
      continue;
    }

    if (!mfrc522.PICC_IsNewCardPresent()) {
      vTaskDelay(xDelay);
      continue;
    }

    if (!mfrc522.PICC_ReadCardSerial()) {
      vTaskDelay(xDelay);
      continue;
    }

    String currentUID = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      currentUID += String(mfrc522.uid.uidByte[i], HEX);
    }

    if (currentUID == lastReadTag && (currentTime - lastReadTime < 5000)) {
      mfrc522.PICC_HaltA();
      mfrc522.PCD_StopCrypto1();
      vTaskDelay(xDelay);
      continue;
    }

    Serial.println("\n------------------------------------------");
    Serial.println("Kart Tespit Edildi!");
    Serial.print("Kart UID: ");
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
      Serial.print(mfrc522.uid.uidByte[i], HEX);
    }
    Serial.println();

    if ((cardOp.isUpdateRequested || writeRequested) && !cardOp.isProcessing) {
      Serial.println("Kimlik işlemi başlatılıyor...");
      if(xSemaphoreTake(cardOpMutex, portMAX_DELAY) == pdTRUE) {
        cardOp.isProcessing = true;
        xSemaphoreGive(cardOpMutex);
      }
      
      byte buffer[18];
      byte size = sizeof(buffer);
      
      status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, BASLANGIC_BLOK, &key, &(mfrc522.uid));
      if (status != MFRC522::STATUS_OK) {
        if(xSemaphoreTake(cardOpMutex, portMAX_DELAY) == pdTRUE) {
          cardOp.status = "Kimlik doğrulama hatası!";
          cardOp.isProcessing = false;
          xSemaphoreGive(cardOpMutex);
        }
        Serial.println("Hata: Kimlik doğrulama başarısız!");
        mfrc522.PICC_HaltA();
        mfrc522.PCD_StopCrypto1();
        vTaskDelay(xDelay);
        continue;
      }

      status = mfrc522.MIFARE_Read(BASLANGIC_BLOK, buffer, &size);
      if (status != MFRC522::STATUS_OK) {
        if(xSemaphoreTake(cardOpMutex, portMAX_DELAY) == pdTRUE) {
          cardOp.status = "Okuma hatası!";
          cardOp.isProcessing = false;
          xSemaphoreGive(cardOpMutex);
        }
        Serial.println("Hata: Kart okunamadı!");
        mfrc522.PICC_HaltA();
        mfrc522.PCD_StopCrypto1();
        vTaskDelay(xDelay);
        continue;
      }

      if (writeRequested) {
        if (!handleKimlikBelirleme(status, buffer, size)) {
          mfrc522.PICC_HaltA();
          mfrc522.PCD_StopCrypto1();
          vTaskDelay(xDelay);
          continue;
        }
      } else if (cardOp.isUpdateRequested) {
        bool isEmpty = true;
        for (byte i = 0; i < 16; i++) {
          if (buffer[i] != 0) {
            isEmpty = false;
            break;
          }
        }

        if (isEmpty) {
          if(xSemaphoreTake(cardOpMutex, portMAX_DELAY) == pdTRUE) {
            cardOp.status = "Kimliksiz Kart! İşlem iptal edildi.";
            cardOp.isUpdateRequested = false;
            cardOp.isProcessing = false;
            xSemaphoreGive(cardOpMutex);
          }
          Serial.println("Durum: Kimliksiz Kart tespit edildi - İşlem sonlandırılıyor");
          mfrc522.PICC_HaltA();
          mfrc522.PCD_StopCrypto1();
          vTaskDelay(xDelay);
          continue;
        }

        Serial.println("Mevcut içerik siliniyor...");
        byte clearBuffer[16] = {0};
        status = mfrc522.MIFARE_Write(BASLANGIC_BLOK, clearBuffer, 16);
        if (status != MFRC522::STATUS_OK) {
          if(xSemaphoreTake(cardOpMutex, portMAX_DELAY) == pdTRUE) {
            cardOp.status = "Silme hatası!";
            cardOp.isProcessing = false;
            xSemaphoreGive(cardOpMutex);
          }
          Serial.println("Hata: Kart içeriği silinemedi!");
          mfrc522.PICC_HaltA();
          mfrc522.PCD_StopCrypto1();
          vTaskDelay(xDelay);
          continue;
        }

        Serial.println("Yeni kimlik yazılıyor: " + cardOp.newValue);
        byte writeBuffer[16] = {0};
        cardOp.newValue.getBytes(writeBuffer, 16);
        status = mfrc522.MIFARE_Write(BASLANGIC_BLOK, writeBuffer, 16);
        if (status != MFRC522::STATUS_OK) {
          if(xSemaphoreTake(cardOpMutex, portMAX_DELAY) == pdTRUE) {
            cardOp.status = "Yazma hatası!";
            cardOp.isProcessing = false;
            xSemaphoreGive(cardOpMutex);
          }
          Serial.println("Hata: Yeni kimlik yazılamadı!");
        } else {
          if(xSemaphoreTake(cardOpMutex, portMAX_DELAY) == pdTRUE) {
            cardOp.status = "Kimlik güncellendi!";
            cardOp.isUpdateRequested = false;
            cardOp.isProcessing = false;
            xSemaphoreGive(cardOpMutex);
          }
          Serial.println("Başarılı: Kimlik güncellendi!");
        }
      }
      
    } else {
      byte buffer[18];
      byte size = sizeof(buffer);
      
      status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, BASLANGIC_BLOK, &key, &(mfrc522.uid));
      if (status == MFRC522::STATUS_OK) {
        status = mfrc522.MIFARE_Read(BASLANGIC_BLOK, buffer, &size);
        if (status == MFRC522::STATUS_OK) {
          TagData newTag;
          String content = "";
          for (byte i = 0; i < 16 && buffer[i] != 0; i++) {
            content += (char)buffer[i];
          }
          newTag.content = content;
          newTag.timestamp = currentTime;
          xQueueSend(tagQueue, &newTag, pdMS_TO_TICKS(100));
          
          Serial.print("Okunan içerik: ");
          Serial.println(content.length() > 0 ? content : "(Boş)");
        } else {
          Serial.println("Hata: Kart okunamadı!");
        }
      } else {
        Serial.println("Hata: Kimlik doğrulama başarısız!");
      }
    }

    Serial.println("------------------------------------------\n");
    lastReadTag = currentUID;
    lastReadTime = currentTime;
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

void dataProcessTask(void* parameter) {
  TagData receivedTag;
  String lastProcessedTag = "";
  unsigned long lastTagTime = 0;
  bool tagActive = false;

  while (1) {
    unsigned long currentTime = millis();
    
    if (!tagActive && xQueueReceive(tagQueue, &receivedTag, pdMS_TO_TICKS(50)) == pdTRUE) {
      if (lastProcessedTag != receivedTag.content) {
        if (xSemaphoreTake(dataAccessMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
          lastProcessedTag = receivedTag.content;
          lastTagTime = currentTime;
          tagActive = true;
          xQueueSend(tagQueue, &receivedTag, 0);
          xSemaphoreGive(dataAccessMutex);
        }
      }
    }

    if (tagActive && (currentTime - lastTagTime >= 2000)) {
      if (xSemaphoreTake(dataAccessMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        TagData emptyTag;
        emptyTag.content = "";
        emptyTag.timestamp = currentTime;
        xQueueReset(tagQueue);
        xQueueSend(tagQueue, &emptyTag, 0);
        lastProcessedTag = "";
        tagActive = false;
        xSemaphoreGive(dataAccessMutex);
      }
    }
    
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void webServerTask(void* parameter) {
  server.begin();
  String lastTag = "";
  bool updateRequested = false;
  bool localWriteRequested = false;
  String selectedOrgan = "KALP";
  
  while (1) {
    WiFiClient client = server.available();
    if (client) {
      String currentLine = "";
      String request = "";
      String currentTag = "";
      
      if (xSemaphoreTake(dataAccessMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        TagData tag;
        if (xQueuePeek(tagQueue, &tag, 0) == pdTRUE) {
          currentTag = tag.content;
          lastTag = currentTag;
        }
        xSemaphoreGive(dataAccessMutex);
      }
      
      while (client.connected()) {
        if (client.available()) {
          char c = client.read();
          request += c;
          if (c == '\n') {
            if (currentLine.length() == 0) {
              bool isVeriKontrol = (request.indexOf("GET /?action=veriKontrol") >= 0);
              bool isKimlikGuncelle = (request.indexOf("GET /?action=kimlikGuncelle") >= 0);
              bool isKimlikBelirleme = (request.indexOf("GET /?action=kimlikBelirleme") >= 0);
              
              if (request.indexOf("GET /cardstatus") >= 0) {
                client.println("HTTP/1.1 200 OK");
                client.println("Content-Type: text/plain");
                client.println("Connection: close");
                client.println();
                
                if(xSemaphoreTake(cardOpMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                  if (cardOp.isProcessing) {
                    client.println("İşlem devam ediyor...");
                  } else if (cardOp.status != "") {
                    client.println(cardOp.status);
                    if (cardOp.status == "Kimlik güncellendi!" || 
                        cardOp.status == "Kimlik belirlendi!" || 
                        cardOp.status.indexOf("iptal edildi") >= 0) {
                      cardOp.status = "";
                      updateRequested = false;
                      localWriteRequested = false;
                      writeRequested = false;
                    }
                  } else if (updateRequested || localWriteRequested) {
                    client.println("Kartı okutun...");
                  } else {
                    client.println("");
                  }
                  xSemaphoreGive(cardOpMutex);
                }
                break;
              }
              
              if (request.indexOf("GET /update") >= 0) {
                updateRequested = true;
                localWriteRequested = false;
                if(xSemaphoreTake(cardOpMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                  cardOp.isUpdateRequested = true;
                  xSemaphoreGive(cardOpMutex);
                }
                int organStart = request.indexOf("organ=") + 6;
                int organEnd = request.indexOf(" ", organStart);
                if (organEnd == -1) organEnd = request.length();
                selectedOrgan = request.substring(organStart, organEnd);
                if(xSemaphoreTake(cardOpMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                  cardOp.newValue = selectedOrgan;
                  xSemaphoreGive(cardOpMutex);
                }
                
                client.println("HTTP/1.1 303 See Other");
                client.println("Location: /?action=kimlikGuncelle");
                client.println("Connection: close");
                client.println();
                break;
              }

              if (request.indexOf("GET /setkimlik") >= 0) {
                localWriteRequested = true;
                updateRequested = false;
                if(xSemaphoreTake(cardOpMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                  cardOp.isUpdateRequested = false;
                  xSemaphoreGive(cardOpMutex);
                }
                int organStart = request.indexOf("organ=") + 6;
                int organEnd = request.indexOf(" ", organStart);
                if (organEnd == -1) organEnd = request.length();
                selectedOrgan = request.substring(organStart, organEnd);
                if(xSemaphoreTake(cardOpMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                  cardOp.newValue = selectedOrgan;
                  xSemaphoreGive(cardOpMutex);
                }
                if(xSemaphoreTake(cardOpMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                  writeRequested = true;
                  xSemaphoreGive(cardOpMutex);
                }
                
                client.println("HTTP/1.1 303 See Other");
                client.println("Location: /?action=kimlikBelirleme");
                client.println("Connection: close");
                client.println();
                break;
              }
              
              client.println("HTTP/1.1 200 OK");
              client.println("Content-type: text/html");
              client.println("Connection: close");
              client.println("Cache-Control: no-store, must-revalidate");
              client.println();
              
              client.print(createHtmlPage(currentTag, isVeriKontrol, isKimlikGuncelle, isKimlikBelirleme, selectedOrgan).c_str());
              break;
            } else {
              currentLine = "";
            }
          } else if (c != '\r') {
            currentLine += c;
          }
        }
      }
      
      delay(1);
      client.stop();
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  
  Serial.println("RFID Test başlıyor...");
  
  SPI.begin();
  mfrc522.PCD_Init();
  delay(50);
  
  byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
  Serial.print("MFRC522 Versiyon: 0x");
  Serial.println(v, HEX);
  if (v == 0x91 || v == 0x92)
    Serial.println("MFRC522 tespit edildi!");
  else
    Serial.println("MFRC522 tespit edilemedi!");

  mfrc522.PCD_DumpVersionToSerial();
  
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);
  
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }
  
  WiFi.softAP(ssid, password);
  Serial.print("AP IP adresi: ");
  Serial.println(WiFi.softAPIP());
  
  tagQueue = xQueueCreate(10, sizeof(TagData));
  dataAccessMutex = xSemaphoreCreateMutex();
  cardOpMutex = xSemaphoreCreateMutex();
  
  xTaskCreatePinnedToCore(rfidTask, "RFID_Task", 4096, NULL, 2, &rfidTaskHandle, 0);
  xTaskCreatePinnedToCore(dataProcessTask, "Data_Process", 4096, NULL, 1, &dataProcessTaskHandle, 0);
  xTaskCreatePinnedToCore(webServerTask, "Web_Server", 8192, NULL, 1, &webServerTaskHandle, 1);
}
void loop() {
}