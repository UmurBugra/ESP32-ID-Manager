#ifndef WEBPAGE_H
#define WEBPAGE_H

// Fonksiyonu String döndürecek şekilde güncelledik (return edilen String'in .c_str()’i kullanılabilir)
String createHtmlPage(const String& tagValue, bool isVeriKontrol, bool isKimlikGuncelle = false, bool isKimlikBelirleme = false, const String& selectedOrgan = "KALP") {
  String html;
  html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
  html += "<title>Tag Arayüzü</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body{font-family:Arial;text-align:center;margin:20px;background-color:#f0f0f0}";
  html += ".organ{display:inline-block;margin:20px;padding:20px;border:3px solid #ff0000;border-radius:15px;background-color:white;transition:all 0.3s ease}";
  html += ".active{border-color:#00ff00 !important;transform:scale(1.2)}";
  html += ".organ-icon{font-size:80px;margin:10px}";
  html += ".organ-title{font-size:18px;font-weight:bold;margin:10px}";
  html += ".button{margin:10px;padding:10px 20px;border-radius:5px;cursor:pointer}";
  html += "#status{margin-top:20px;padding:10px;color:red;font-weight:bold}";
  html += "</style></head><body>";

  if (isVeriKontrol) {
    html += "<h2>Organ Durumu</h2>";
    
    String kalp = (tagValue == "KALP") ? "active" : "";
    String akciger = (tagValue == "AKCIGER") ? "active" : "";
    String bagirsak = (tagValue == "BAGIRSAK") ? "active" : "";
    
    html += "<div id='kalp' class='organ " + kalp + "'>";
    html += "<div class='organ-title'>Kalp " + String((tagValue == "KALP") ? "✓" : "×") + "</div>";
    html += "<div class='organ-icon'>❤️</div>";
    html += "</div>";
    
    html += "<div id='akciger' class='organ " + akciger + "'>";
    html += "<div class='organ-title'>Akciğer " + String((tagValue == "AKCIGER") ? "✓" : "×") + "</div>";
    html += "<div class='organ-icon'>🫁</div>";
    html += "</div>";
    
    html += "<div id='bagirsak' class='organ " + bagirsak + "'>";
    html += "<div class='organ-title'>Bağırsak " + String((tagValue == "BAGIRSAK") ? "✓" : "×") + "</div>";
    html += "<div class='organ-icon'>🍀</div>";
    html += "</div>";
    
    html += "<script>";
    html += "function checkStatus() {";
    html += "  fetch(window.location.href).then(response => response.text()).then(html => {";
    html += "    const tempDiv = document.createElement('div');";
    html += "    tempDiv.innerHTML = html;";
    html += "    const activeElement = tempDiv.querySelector('.active');";
    html += "    const currentActive = document.querySelector('.active');";
    
    html += "    if (activeElement && !currentActive) {";
    html += "      const organId = activeElement.id;";
    html += "      const targetElement = document.getElementById(organId);";
    html += "      if (targetElement) {";
    html += "        targetElement.classList.add('active');";
    html += "        targetElement.querySelector('.organ-title').innerHTML = targetElement.querySelector('.organ-title').innerHTML.replace('×', '✓');";
    html += "        setTimeout(() => {";
    html += "          targetElement.classList.remove('active');";
    html += "          targetElement.querySelector('.organ-title').innerHTML = targetElement.querySelector('.organ-title').innerHTML.replace('✓', '×');";
    html += "        }, 2000);";
    html += "      }";
    html += "    }";
    html += "  });";
    html += "}";
    html += "setInterval(checkStatus, 500);";
    html += "</script>";
        
    html += "<form action='/' method='GET'><input type='submit' value='Geri Dön' class='button'></form>";
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
    html += "<input type='submit' value='Kimlik Belirle' class='button'>";
    html += "</form>";
    html += "</div>";
    
    html += "<div id='status'></div>";
    
    html += "<script>";
    html += "let lastStatus = '';";
    html += "function checkCardStatus() {";
    html += "  fetch('/cardstatus').then(response => response.text()).then(status => {";
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
    html += "<input type='submit' value='Kimlik Yaz' class='button'>";
    html += "</form>";
    html += "</div>";
    
    html += "<div id='status'></div>";
    
    html += "<script>";
    html += "function checkCardStatus() {";
    html += "  fetch('/cardstatus').then(response => response.text()).then(status => {";
    html += "    document.getElementById('status').innerHTML = status;";
    html += "  });";
    html += "}";
    html += "setInterval(checkCardStatus, 500);";
    html += "</script>";
    
    html += "<form action='/' method='GET'><input type='submit' value='Geri Dön' class='button'></form>";
  }
  else {
    html += "<h2>Ana Menü</h2>";
    
    // Veri Kontrol butonu
    html += "<form action='/' method='GET' style='margin:10px;'>";
    html += "<input type='hidden' name='action' value='veriKontrol'>";
    html += "<input type='submit' value='Veri Kontrol' class='button'>";
    html += "</form>";
    
    // Kimlik Güncelle butonu
    html += "<form action='/' method='GET' style='margin:10px;'>";
    html += "<input type='hidden' name='action' value='kimlikGuncelle'>";
    html += "<input type='submit' value='Kimlik Güncelle' class='button'>";
    html += "</form>";
    
    // Kimlik Belirleme butonu
    html += "<form action='/' method='GET' style='margin:10px;'>";
    html += "<input type='hidden' name='action' value='kimlikBelirleme'>";
    html += "<input type='submit' value='Kimlik Belirleme' class='button'>";
    html += "</form>";
  }
  
  html += "</body></html>";
  return html;
}

#endif
