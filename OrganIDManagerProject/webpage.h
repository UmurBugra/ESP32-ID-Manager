#ifndef WEBPAGE_H
#define WEBPAGE_H

// Fonksiyonu String döndürecek şekilde güncelledik (return edilen String'in .c_str()'i kullanılabilir)
String createHtmlPage(const String& tagValue, bool isVeriKontrol, bool isKimlikGuncelle = false, bool isKimlikBelirleme = false, const String& selectedOrgan = "KALP") {
  String html;
  html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
  html += "<title>Organ Takip Sistemi</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  
  // Meta refresh kaldırıldı - otomatik yenilemeyi durdurmak için
  
  html += "<style>";
  // Dieter Rams stili - minimalist, fonksiyonel, net - Menlo font eklendi
  html += "body{font-family:Menlo,monospace;text-align:center;margin:0;padding:40px 20px;background-color:#f8f8f8;color:#333}";
  html += "h2{font-weight:normal;font-size:24px;margin-bottom:40px;letter-spacing:1px}";
  
  // Organlar için yeni tasarım
  html += ".organ-container{display:flex;justify-content:center;gap:30px;flex-wrap:wrap;margin-bottom:40px}";
  html += ".organ{display:flex;flex-direction:column;align-items:center;width:120px;height:150px;padding:25px 15px;border-radius:2px;background-color:white;box-shadow:0 1px 3px rgba(0,0,0,0.1);transition:all 0.2s ease}";
  html += ".active{background-color:#e3f2fd;box-shadow:0 2px 8px rgba(0,0,0,0.15)}";
  html += ".organ-icon{font-size:50px;margin:15px 0}";
  html += ".organ-title{font-size:14px;color:#555;margin-top:10px;text-transform:uppercase;letter-spacing:0.5px}";
  
  // Butonlar için yeni tasarım
  html += ".button{background-color:#eaeaea;border:none;padding:12px 25px;margin:8px;color:#333;font-size:14px;letter-spacing:0.5px;border-radius:2px;cursor:pointer;transition:background-color 0.2s}";
  html += ".button:hover{background-color:#d0d0d0}";
  html += ".primary-button{background-color:#333;color:#fff}";
  html += ".primary-button:hover{background-color:#555}";
  
  // Durum mesajı için yeni tasarım
  html += "#status{margin-top:30px;padding:10px;color:#d32f2f;font-size:14px;height:20px}";
  
  // Seçim kutusu için yeni tasarım
  html += "select{background-color:#eaeaea;border:none;padding:12px 25px;margin:8px;color:#333;font-size:14px;border-radius:2px;-webkit-appearance:none;-moz-appearance:none;appearance:none;background-image:url('data:image/svg+xml;utf8,<svg fill=\"%23333\" height=\"24\" viewBox=\"0 0 24 24\" width=\"24\" xmlns=\"http://www.w3.org/2000/svg\"><path d=\"M7 10l5 5 5-5z\"/><path d=\"M0 0h24v24H0z\" fill=\"none\"/></svg>');background-repeat:no-repeat;background-position:right 10px center;padding-right:30px}";
  
  // Durum göstergesi için stil
  html += "#durum-gostergesi{position:fixed;bottom:10px;right:10px;background-color:#f8f9fa;border:1px solid #dee2e6;border-radius:4px;padding:5px 10px;font-size:12px;color:#6c757d;z-index:100}";
  
  // Yenileme butonu kaldırıldı
  
  html += "</style></head><body>";

  if (isVeriKontrol) {
    html += "<h2>Organ Durumu</h2>";
    
    String kalp = (tagValue == "KALP") ? "active" : "";
    String akciger = (tagValue == "AKCIGER") ? "active" : "";
    String bagirsak = (tagValue == "BAGIRSAK") ? "active" : "";
    
    html += "<div class='organ-container'>";
    
    html += "<div id='kalp' class='organ " + kalp + "'>";
    html += "<div class='organ-icon'>❤️</div>";
    html += "<div class='organ-title'>Kalp " + String((tagValue == "KALP") ? "•" : "") + "</div>";
    html += "</div>";
    
    html += "<div id='akciger' class='organ " + akciger + "'>";
    html += "<div class='organ-icon'>🫁</div>";
    html += "<div class='organ-title'>Akciğer " + String((tagValue == "AKCIGER") ? "•" : "") + "</div>";
    html += "</div>";
    
    html += "<div id='bagirsak' class='organ " + bagirsak + "'>";
    html += "<div class='organ-icon'>🍀</div>";
    html += "<div class='organ-title'>Bağırsak " + String((tagValue == "BAGIRSAK") ? "•" : "") + "</div>";
    html += "</div>";
    
    html += "</div>"; // organ-container end
    
    html += "<form action='/' method='GET'><input type='submit' value='Geri Dön' class='button'></form>";
    
    // Manuel yenileme butonu kaldırıldı
    
    // Durum göstergesi ekle
    html += "<div id='durum-gostergesi'>Son Görülen: " + tagValue + "</div>";
    
    // Yeni JavaScript kodu - Sürekli karşılaştırma ve sadece değişiklik durumunda sayfa yenileme
    html += "<script>";
    html += "var lastKnownTag = '" + tagValue + "';";
    
    // Durum kontrolü için sürekli polling fonksiyonu
    html += "function pollTagStatus() {";
    html += "  var xhr = new XMLHttpRequest();";
    html += "  xhr.open('GET', '/tag-durumu?' + new Date().getTime(), true);";
    html += "  xhr.onload = function() {";
    html += "    if (xhr.status === 200) {";
    html += "      var currentTag = xhr.responseText;";
    html += "      document.getElementById('durum-gostergesi').textContent = 'Son Görülen: ' + (currentTag ? currentTag : 'Boş');";
    html += "      if (currentTag !== lastKnownTag) {";
    html += "        console.log('Değişiklik tespit edildi! Eski: ' + lastKnownTag + ', Yeni: ' + currentTag);";
    html += "        window.location.reload();";
    html += "      }";
    html += "    }";
    html += "  };";
    html += "  xhr.onerror = function() {";
    html += "    console.error('Durum kontrolü sırasında hata oluştu');";
    html += "  };";
    html += "  xhr.send();";
    html += "}";
    
    // Her 500ms'de bir durum kontrolü yap
    html += "setInterval(pollTagStatus, 500);";
    html += "</script>";
  } 
  else if (isKimlikGuncelle) {
    html += "<h2>Kimlik Güncelleme</h2>";
    
    html += "<div class='organ-selection'>";
    html += "<form action='/update' method='GET'>";
    html += "<select name='organ' class='button'>";
    html += "<option value='KALP'" + String(selectedOrgan == "KALP" ? " selected" : "") + ">Kalp</option>";
    html += "<option value='AKCIGER'" + String(selectedOrgan == "AKCIGER" ? " selected" : "") + ">Akciğer</option>";
    html += "<option value='BAGIRSAK'" + String(selectedOrgan == "BAGIRSAK" ? " selected" : "") + ">Bağırsak</option>";
    html += "</select>";
    html += "<input type='submit' value='Kimlik Belirle' class='button primary-button'>";
    html += "</form>";
    html += "</div>";
    
    html += "<div id='status'></div>";
    
    html += "<script>";
    html += "let lastStatus = '';";
    html += "function checkCardStatus() {";
    html += "  fetch('/cardstatus?' + new Date().getTime()).then(response => response.text()).then(status => {";
    html += "    const statusElement = document.getElementById('status');";
    html += "    if (status === 'Kimliksiz Kart! İşlem iptal edildi.') {";
    html += "      if (lastStatus !== status) {";
    html += "        statusElement.innerHTML = status;";
    html += "        lastStatus = status;";
    html += "        setTimeout(() => {";
    html += "          statusElement.innerHTML = '';";
    html += "          lastStatus = '';";
    html += "        }, 2000);";
    html += "      }";
    html += "    } else {";
    html += "      statusElement.innerHTML = status;";
    html += "      lastStatus = status;";
    html += "    }";
    html += "  });";
    html += "}";
    html += "setInterval(checkCardStatus, 500);";
    html += "</script>";
    
    html += "<form action='/' method='GET'><input type='submit' value='Geri Dön' class='button'></form>";
  }
  else if (isKimlikBelirleme) {
    html += "<h2>Kimlik Belirleme</h2>";
    
    html += "<div class='organ-selection'>";
    html += "<form action='/setkimlik' method='GET'>";
    html += "<select name='organ' class='button'>";
    html += "<option value='KALP'" + String(selectedOrgan == "KALP" ? " selected" : "") + ">Kalp</option>";
    html += "<option value='AKCIGER'" + String(selectedOrgan == "AKCIGER" ? " selected" : "") + ">Akciğer</option>";
    html += "<option value='BAGIRSAK'" + String(selectedOrgan == "BAGIRSAK" ? " selected" : "") + ">Bağırsak</option>";
    html += "</select>";
    html += "<input type='submit' value='Kimlik Yaz' class='button primary-button'>";
    html += "</form>";
    html += "</div>";
    
    html += "<div id='status'></div>";
    
    html += "<script>";
    html += "function checkCardStatus() {";
    html += "  fetch('/cardstatus?' + new Date().getTime()).then(response => response.text()).then(status => {";
    html += "    document.getElementById('status').innerHTML = status;";
    html += "  });";
    html += "}";
    html += "setInterval(checkCardStatus, 500);";
    html += "</script>";
    
    html += "<form action='/' method='GET'><input type='submit' value='Geri Dön' class='button'></form>";
  }
  else {
    html += "<h2>Organ Takip Sistemi</h2>";
    
    html += "<div style='display:flex;flex-direction:column;align-items:center;max-width:300px;margin:0 auto'>";
    
    // Veri Kontrol butonu
    html += "<form action='/' method='GET' style='width:100%;margin-bottom:15px'>";
    html += "<input type='hidden' name='action' value='veriKontrol'>";
    html += "<input type='submit' value='Veri Kontrol' class='button' style='width:100%'>";
    html += "</form>";
    
    // Kimlik Güncelle butonu
    html += "<form action='/' method='GET' style='width:100%;margin-bottom:15px'>";
    html += "<input type='hidden' name='action' value='kimlikGuncelle'>";
    html += "<input type='submit' value='Kimlik Güncelle' class='button' style='width:100%'>";
    html += "</form>";
    
    // Kimlik Belirleme butonu
    html += "<form action='/' method='GET' style='width:100%'>";
    html += "<input type='hidden' name='action' value='kimlikBelirleme'>";
    html += "<input type='submit' value='Kimlik Belirleme' class='button primary-button' style='width:100%'>";
    html += "</form>";
    
    html += "</div>";
  }
  
  html += "</body></html>";
  return html;
}
#endif