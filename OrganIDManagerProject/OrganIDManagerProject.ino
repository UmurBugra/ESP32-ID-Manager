#include <SPI.h>
#include <MFRC522.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <WiFi.h>
#include <WebServer.h>
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
const char* password = "nwakaeme";    // Respect to Anthony Nnaduzor Nwakaeme
WebServer server(80); 


String currentTagValue = "";

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

// /tag-durumu endpoint kontrol
void handleTagDurumu() {
  server.send(200, "text/plain", currentTagValue);
}

// Boş bir karta kimlik yazma fonksiyonu
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

// RFID işlemleri
void rfidTask(void* parameter) {
  String lastReadTag = "";
  String currentCardContent = "";
  unsigned long lastReadTime = 0;
  unsigned long lastCardCheck = 0;
  bool cardPresent = false;
  const unsigned long CHECK_INTERVAL = 100;
  const unsigned long CARD_OP_CHECK_INTERVAL = 300; 
  unsigned long lastCardOpCheck = 0; 
 
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }

  mfrc522.PCD_Init();
  
  for (;;) {
    unsigned long currentTime = millis();
    
    bool isCardOperation = false;
    if(xSemaphoreTake(cardOpMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      isCardOperation = cardOp.isUpdateRequested || writeRequested;
      xSemaphoreGive(cardOpMutex);
    }
    
    if (isCardOperation && (currentTime - lastCardOpCheck >= CARD_OP_CHECK_INTERVAL)) {
      lastCardOpCheck = currentTime;
      
      mfrc522.PCD_Init();
      
      if (!cardOp.isProcessing) {
        if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
          Serial.println("\n------------------------------------------");
          Serial.println("Kimlik işlemi için kart algılandı.");
          Serial.print("Kart UID: ");
          for (byte i = 0; i < mfrc522.uid.size; i++) {
            Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
            Serial.print(mfrc522.uid.uidByte[i], HEX);
          }
          Serial.println();
          
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
            vTaskDelay(pdMS_TO_TICKS(50));
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
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
          }
          
          if (writeRequested) {
            if (!handleKimlikBelirleme(status, buffer, size)) {
              mfrc522.PICC_HaltA();
              mfrc522.PCD_StopCrypto1();
              vTaskDelay(pdMS_TO_TICKS(50));
              continue;
            }
            
            currentCardContent = cardOp.newValue;
            // Global değişkeni güncelle
            currentTagValue = cardOp.newValue;
            
            TagData newTag;
            newTag.content = cardOp.newValue;
            newTag.timestamp = currentTime;
            
            xQueueReset(tagQueue);
            xQueueSend(tagQueue, &newTag, pdMS_TO_TICKS(100));
            
            cardPresent = true;
            lastCardCheck = currentTime;
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
              vTaskDelay(pdMS_TO_TICKS(50));
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
              vTaskDelay(pdMS_TO_TICKS(50));
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
              
              currentCardContent = cardOp.newValue;
              // Global değişkeni güncelle
              currentTagValue = cardOp.newValue;
              
              TagData newTag;
              newTag.content = cardOp.newValue;
              newTag.timestamp = currentTime;
              
              xQueueReset(tagQueue);
              xQueueSend(tagQueue, &newTag, pdMS_TO_TICKS(100));
              
              cardPresent = true;
              lastCardCheck = currentTime;
            }
          }
          
          mfrc522.PICC_HaltA();
          mfrc522.PCD_StopCrypto1();
        }
      }
    }
    
    if (!isCardOperation && (currentTime - lastReadTime >= CHECK_INTERVAL)) {
      lastReadTime = currentTime;

      mfrc522.PCD_Init();
  
      if (mfrc522.PICC_IsNewCardPresent()) {
        if (mfrc522.PICC_ReadCardSerial()) {
   
          String currentUID = "";
          for (byte i = 0; i < mfrc522.uid.size; i++) {
            currentUID += String(mfrc522.uid.uidByte[i], HEX);
          }
          
          if (!cardPresent) {
            Serial.println("\n------------------------------------------");
            Serial.println("Kart Tespit Edildi!");
            Serial.print("Kart UID: ");
            for (byte i = 0; i < mfrc522.uid.size; i++) {
              Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
              Serial.print(mfrc522.uid.uidByte[i], HEX);
            }
            Serial.println();
          }
          
          lastCardCheck = currentTime;
          
          byte buffer[18];
          byte size = sizeof(buffer);
      
          status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, BASLANGIC_BLOK, &key, &(mfrc522.uid));
          if (status == MFRC522::STATUS_OK) {
      
            status = mfrc522.MIFARE_Read(BASLANGIC_BLOK, buffer, &size);
            if (status == MFRC522::STATUS_OK) {
  
              String content = "";
              for (byte i = 0; i < 16 && buffer[i] != 0; i++) {
                content += (char)buffer[i];
              }
              
              if (!cardPresent || content != currentCardContent) {
                currentCardContent = content;
                // Global değişkeni güncelle
                currentTagValue = content;
                
                Serial.print("Kart algılandı. Okunan içerik: ");
                Serial.println(content.length() > 0 ? content : "(Boş)");
    
                TagData newTag;
                newTag.content = content;
                newTag.timestamp = currentTime;
                
                xQueueReset(tagQueue);
                xQueueSend(tagQueue, &newTag, pdMS_TO_TICKS(100));
                
                cardPresent = true;
              }
            }
          }
          
          mfrc522.PICC_HaltA();
          mfrc522.PCD_StopCrypto1();
        }
      }
      
      if (cardPresent && (currentTime - lastCardCheck >= 1000)) {

        if (currentCardContent.length() > 0) {
          Serial.println("Kart algılanamıyor, sıfırlanıyor.");
          
          TagData emptyTag;
          emptyTag.content = "";
          emptyTag.timestamp = currentTime;
          
          xQueueReset(tagQueue);
          xQueueSend(tagQueue, &emptyTag, 0);
          
          currentCardContent = "";
          // Global değişkeni temizle
          currentTagValue = "";
          cardPresent = false;
        }
      }
    }
    
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// Veri işleme
void dataProcessTask(void* parameter) {
  TagData receivedTag;
  String lastProcessedTag = "";
  unsigned long lastTagTime = 0;
  bool tagActive = false;

  while (1) {
    unsigned long currentTime = millis();
    
    if (xQueueReceive(tagQueue, &receivedTag, pdMS_TO_TICKS(50)) == pdTRUE) {
      if (xSemaphoreTake(dataAccessMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (receivedTag.content != lastProcessedTag) {
          if (receivedTag.content.length() > 0) {
        
            if (!tagActive) {
              Serial.println("------------------------------------------");
              Serial.println("Kart aktif.");
              tagActive = true;
            }
          } else {
         
            if (tagActive) {
              Serial.println("Kart deaktif edildi.");
              tagActive = false;
            }
          }
          
          lastProcessedTag = receivedTag.content;
        }
        
        lastTagTime = currentTime;
        xQueueSend(tagQueue, &receivedTag, 0);
        
        xSemaphoreGive(dataAccessMutex);
      }
    }
    
    bool isCardOperation = false;
    if(xSemaphoreTake(cardOpMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      isCardOperation = cardOp.isUpdateRequested || writeRequested;
      xSemaphoreGive(cardOpMutex);
    }
    if (!isCardOperation && tagActive && (currentTime - lastTagTime >= 2000)) {
      if (xSemaphoreTake(dataAccessMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        TagData emptyTag;
        emptyTag.content = "";
        emptyTag.timestamp = currentTime;
        xQueueReset(tagQueue);
        xQueueSend(tagQueue, &emptyTag, 0);
        lastProcessedTag = "";
        tagActive = false;
        // Global değişkeni temizle
        currentTagValue = "";
        xSemaphoreGive(dataAccessMutex);
      }
    }
    
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// Arayüzü döndürür
void handleRoot() {
  String currentTag = "";
  
  if (xSemaphoreTake(dataAccessMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    TagData tag;
    if (xQueuePeek(tagQueue, &tag, 0) == pdTRUE) {
      currentTag = tag.content;
    }
    xSemaphoreGive(dataAccessMutex);
  }
  
  bool isVeriKontrol = (server.arg("action") == "veriKontrol");
  bool isKimlikGuncelle = (server.arg("action") == "kimlikGuncelle");
  bool isKimlikBelirleme = (server.arg("action") == "kimlikBelirleme");
  String selectedOrgan = server.hasArg("organ") ? server.arg("organ") : "KALP";
  
  server.send(200, "text/html", createHtmlPage(currentTag, isVeriKontrol, isKimlikGuncelle, isKimlikBelirleme, selectedOrgan).c_str());
}

// Kart işlem durumunu takip eder
void handleCardStatus() {
  String status = "";
  
  if(xSemaphoreTake(cardOpMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    if (cardOp.isProcessing) {
      status = "İşlem devam ediyor...";
    } else if (cardOp.status != "") {
      status = cardOp.status;
      if (cardOp.status == "Kimlik güncellendi!" || 
          cardOp.status == "Kimlik belirlendi!" || 
          cardOp.status.indexOf("iptal edildi") >= 0) {
        cardOp.status = "";
      }
    } else if (cardOp.isUpdateRequested || writeRequested) {
      status = "Kartı okutun...";
    }
    xSemaphoreGive(cardOpMutex);
  }
  
  server.send(200, "text/plain", status);
}

// Kimlik güncelleme
void handleUpdate() {
  if(xSemaphoreTake(cardOpMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    cardOp.isUpdateRequested = true;
    cardOp.newValue = server.arg("organ");
    xSemaphoreGive(cardOpMutex);
  }
  
  server.sendHeader("Location", "/?action=kimlikGuncelle", true);
  server.send(303, "text/plain", "");
}

// Boş karta kimlik yazma
void handleSetKimlik() {
  if(xSemaphoreTake(cardOpMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    cardOp.newValue = server.arg("organ");
    writeRequested = true;
    xSemaphoreGive(cardOpMutex);
  }
  
  server.sendHeader("Location", "/?action=kimlikBelirleme", true);
  server.send(303, "text/plain", "");
}

// WebServer işlemleri
void webServerTask(void* parameter) {
  // Route'ları tanımla
  server.on("/", HTTP_GET, handleRoot);
  server.on("/cardstatus", HTTP_GET, handleCardStatus);
  server.on("/tag-durumu", HTTP_GET, handleTagDurumu);
  server.on("/update", HTTP_GET, handleUpdate);
  server.on("/setkimlik", HTTP_GET, handleSetKimlik);
  
  server.begin();
  
  while (1) {
    server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(20));  
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