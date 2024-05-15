#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>


#include "WiFi.h"
#include "esp_camera.h"
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "soc/soc.h"           // Disable brownour problems
#include "soc/rtc_cntl_reg.h"  // Disable brownour problems
#include "driver/rtc_io.h"
#include <ESPAsyncWebServer.h>
#include <StringArray.h>
#include <SPIFFS.h>
#include <FS.h>


#define CAMERA_MODEL_AI_THINKER

#define TFT_CS 15
#define TFT_RST 12
#define TFT_DC 2
#define TFT_MOSI 13
#define TFT_SCLK 14
//A0-2

const char* ssid = "Wallababa";
const char* password = "12345678";

//const char* ssid = ".";
//const char* password = "92345679";

//const char* ssid = "PPC185D2-2.4G";
//const char* password = "12345678";

//const char* ssid = "PPC185D2-5G";
//const char* password = "12345678";


//const char* ssid = "VivoS";
//const char* password = "12345678";

String ipAddrDis = "";

const int buttonPin = 4;


int press_status = 0;
int press_count = 0;
int press_millis = 0;
int start_millis = 0;



int buttonState = 0;

int penMode = -1;

bool shouldChangeImage = false;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

//receive_text

boolean takeNewPhoto = false;
boolean imageAvailable = false;


// Photo File Name to save in SPIFFS
#define FILE_PHOTO "/photo.jpg"

// OV2640 camera module pins (CAMERA_MODEL_AI_THINKER)
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
</head>
<body>
  <div id="container">
    <div class="text-container">
      <h1 class="h1_1">SMART PEN</h1>
      <h2 class="h2_1"><u>WEB SERVER</u></h2>
      <h3 class="h3_1">CEREBRO</h3>
      <p class="p1_1">"Project Cerebro" is a smart pen with an ESP32 CAM for scanning. It captures handwritten text or printed words, providing instant meaning or translation on a TFT display. Its compact design ensures portability, making it ideal for students and professionals. With advanced algorithms, Cerebro revolutionizes information access, enhancing productivity and learning efficiency.</p>
      <div class="button-container">
        <button onclick="rotatePhoto();">ROTATE</button>
        <button onclick="capturePhoto()">CAPTURE PHOTO</button>
        <button onclick="reloadPage()">REFRESH PAGE</button>
        <button class="download-button" onclick="downloadPhoto()">DOWNLOAD PHOTO</button>
      </div>
    </div>
    <div class="photo-container">
      <img src="saved_photo" id="photo" width="100%">
    </div>
  </div>
</body>
<script>
  var deg = 0;

  function capturePhoto() {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', "/capture", true);
    xhr.send();
  }

  function rotatePhoto() {
    var img = document.getElementById("photo");
    deg += 90;
    if (isOdd(deg / 90)) {
      document.getElementById("container").className = "vert";
    } else {
      document.getElementById("container").className = "hori";
    }
    img.style.transform = "rotate(" + deg + "deg)";
  }

  function reloadPage(){
    location.reload();
  }


  function downloadPhoto() {
    var img = document.getElementById("photo");
    var url = img.src;
    var a = document.createElement('a');
    a.href = url;
    a.download = 'photo.jpg';
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
  }


</script>
</html>
)rawliteral";


Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);


void setup(void) {
  // in setup()
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  pinMode(buttonPin, INPUT);


  tft.initR(INITR_144GREENTAB);

  delay(500);
  displayInitially();

  displaySerialWork();
  WiFi.begin(ssid, password);  // Connect to Wi-Fi
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    tftSerialPrintln("Connecting to WiFi...");
  }
  if (!SPIFFS.begin(true)) {
    tftSerialPrintln("An Error has occurred while mounting SPIFFS");
    ESP.restart();
  } else {
    delay(500);
    tftSerialPrintln("SPIFFS mounted successfully");
  }

  // Print ESP32 Local IP Address
  tftSerialPrintln("IP Address: http://");
  ipAddrDis = (WiFi.localIP()).toString();
  tftSerialPrintln(ipAddrDis);

  // Turn-off the 'brownout detector'
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  // OV2640 camera module
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;

  if (psramFound()) {
    //config.frame_size = FRAMESIZE_UXGA;
    //config.frame_size = FRAMESIZE_SVGA;
    //config.frame_size = FRAMESIZE_QVGA;
    config.frame_size = FRAMESIZE_VGA;

    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    tftSerialPrintln("Camera init failed with error 0x%x" + String(err, HEX));
    ESP.restart();
  }
  sensor_t* s = esp_camera_sensor_get();
  s->set_special_effect(s, 2);


  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send_P(200, "text/html", index_html);
  });

  server.on("/capture", HTTP_GET, [](AsyncWebServerRequest* request) {
    takeNewPhoto = true;
    request->send_P(200, "text/plain", "Taking Photo");
  });

  server.on("/saved_photo", HTTP_GET, [](AsyncWebServerRequest* request) {
    /*
    request->send(SPIFFS, FILE_PHOTO, "image/jpg", false);
    shouldChangeImage = false;
    */
    AsyncWebServerResponse *response = request->beginResponse(SPIFFS, FILE_PHOTO, "image/jpg", true);
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response->addHeader("Pragma", "no-cache");
    response->addHeader("Expires", "-1");
    request->send(response);
    shouldChangeImage = false;
  });

    server.on("/saved_type", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/plain", String(penMode));
  });

  server.on("/status_tell", HTTP_GET, [](AsyncWebServerRequest* request) {
    // Assuming `imageAvailable` is a boolean indicating whether an image is available
    request->send_P(200, "text/plain", imageAvailable ? PSTR("true") : PSTR("false"));
  });

  // Handle /check-image-change endpoint
  server.on("/check-image-change", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (shouldChangeImage) {
      shouldChangeImage = false;                         // Reset the flag
      request->send(200, "text/plain", "Change Image");  // Respond positively to indicate image should be changed
    } else {
      request->send(204);  // No content to indicate image should not be changed
    }
  });

  server.on("/receive_text", HTTP_POST, [](AsyncWebServerRequest* request) {
    String message;
    String modeType;
    String wordDetected;
    String wordCorrect;
    String wordMeaning;
    if ((request->hasParam("message", true))&&(request->hasParam("modeType", true))&&(request->hasParam("wordDetected", true))&&(request->hasParam("wordCorrect", true))&&(request->hasParam("wordMeaning", true))) {
      message = request->getParam("message", true)->value();
      modeType=  request->getParam("modeType", true)->value();
      wordDetected=  request->getParam("wordDetected", true)->value();
      wordCorrect=  request->getParam("wordCorrect", true)->value();
      wordMeaning=  request->getParam("wordMeaning", true)->value();
      tftSerialCPrintln("Message Recieved",0);
      tftSerialCPrintln("Received message: " + message,0);
      if (message[0] == '1') {
        imageAvailable = false;
      }
      if(message[1]=='1'){
        
        tftSerialCPrintln("recognized text: "+ wordDetected,0);
        tftSerialCPrintln("WORD  : "+wordCorrect,1);
        tftSerialCPrintln(wordMeaning,0);
      }
      request->send(200, "text/plain", "Message received");
    } else {
      tftSerialCPrintln("Message Not Recieved",0);
      request->send(400, "text/plain", "No message received");
    }
  });

  // Start server
  server.begin();
  start_millis = millis();
  press_millis = 0;
  //SPIFFS.format();
}



void loop() {
  if (takeNewPhoto) {
    capturePhotoSaveSpiffs();
    takeNewPhoto = false;
  }
  delay(1);


  buttonState = digitalRead(buttonPin);



  if (press_millis == 0) {
    if (buttonState == HIGH) {
      press_status = 1;
      press_count = 1;
      press_millis = millis();
    }
  } else if (millis() - press_millis < 2000) {
    if (buttonState == HIGH && press_status == 0) {
      press_count = press_count + 1;
    } else if (buttonState == LOW) {
      press_status = 0;
    }
  } else if (millis() - press_millis >= 2000) {
    if (buttonState == HIGH && press_count == 1) {
      tftSerialCPrintln("Long Button Pressed",1);
      longButtonPressed();
      delay(1000);
    } else if (buttonState == HIGH && press_count >= 2) {
      tftSerialCPrintln("Button Clicked 2x",1);
      if (penMode == 3) {
        penMode = 0;
      } else {
        penMode = penMode + 1;
      }

      penModeIn();
    } else if (buttonState == LOW && press_count == 1) {
      tftSerialCPrintln("Button Clicked",1);
    } else if (buttonState == LOW && press_count >= 2) {
      tftSerialCPrintln("Button Clicked 2x",1);
      if (penMode == 3) {
        penMode = 0;
      } else {
        penMode = penMode + 1;
      }
      penModeIn();
    }
    press_status = 0;
    press_count = 0;
    press_millis = 0;
  }
}
// Check if photo capture was successful
bool checkPhoto(fs::FS& fs) {
  File f_pic = fs.open(FILE_PHOTO);
  unsigned int pic_sz = f_pic.size();
  return (pic_sz > 100);
}

// Capture Photo and Save it to SPIFFS
void capturePhotoSaveSpiffs(void) {
  

  camera_fb_t* fb = NULL;  // pointer
  bool ok = 0;             // Boolean indicating if the picture has been taken correctly

  do {
    // Take a photo with the camera
    tftSerialCPrintln("Taking a photo...",0);


    fb = esp_camera_fb_get();
   
    if (!fb) {
      tftSerialCPrintln("Camera capture failed",0);
      return;
    }

    // Photo file name
    tftSerialCPrintln("Picture file name: %s\n" + String(FILE_PHOTO),0);
    File file = SPIFFS.open(FILE_PHOTO, FILE_WRITE);

    // Insert the data in the photo file
    if (!file) {
      tftSerialCPrintln("Failed to open file in writing mode",0);
    } else {
      file.write(fb->buf, fb->len);  // payload (image), payload length
      tftSerialCPrintln("The picture has been saved in ",0);
      tftSerialCPrintln(FILE_PHOTO,0);
      tftSerialCPrintln(" - Size: " + String(file.size()),0);
      tftSerialCPrintln(" bytes",0);
    }
    // Close the file
    file.close();
    esp_camera_fb_return(fb);

    // check if file has been correctly saved in SPIFFS
    ok = checkPhoto(SPIFFS);
  } while (!ok);
  shouldChangeImage = true;
  imageAvailable = true;
}


void displayInitially() {
  tft.setTextWrap(false);

  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(4, 50);
  tft.setRotation(3);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(3);
  tft.println("CEREBRO");

  delay(2000);

  tft.fillScreen(ST77XX_WHITE);
  tft.setCursor(25, 35);
  tft.setRotation(3);
  tft.setTextColor(ST77XX_RED);
  tft.setTextSize(3);
  tft.println("SMART");
  tft.setTextSize(3);
  tft.setCursor(40, 65);
  tft.println("PEN");

  delay(2000);
}


void displaySerialWork() {
  tft.setTextWrap(true);
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 0);
  tft.setRotation(3);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
}



void tftSerialPrintln(String abc) {
  if (tft.getCursorY() >= tft.width()) {
    tft.fillScreen(ST77XX_BLACK);
    tft.setCursor(0, 0);
  }
  tft.setTextColor(ST77XX_CYAN);
  tft.print("-> ");
  tft.setTextColor(ST77XX_GREEN);
  tft.println(abc);
}
void penModeIn() {
  if (penMode == 0) {
    penModeSettings0();
  } else if (penMode == 1) {
    penModeMeaning1();
  } else if (penMode == 2) {
    penModeCalculation2();
  } else if (penMode == 3) {
    penModeGraph3();
  }
}  

void penModeSettings0() {

  unsigned long bgHexValue = ST7735_RED;
  unsigned long tx1HexValue = ST7735_CYAN;
  unsigned long tx2HexValue = ST7735_YELLOW;

  tft.fillScreen(bgHexValue);
  tft.setCursor(0, 3);
  tft.setTextColor(tx1HexValue);
  tft.setTextSize(2);
  tft.println("  SETTING");
  tft.setTextSize(1);
  tft.drawLine(0,20,tft.height(),20,ST7735_BLACK);
  tft.setCursor(0, 30);
  if (tft.getCursorY() >= tft.width()) {
    tft.fillScreen(bgHexValue);
    tft.setCursor(0, 3);
    tft.setTextColor(tx2HexValue);
    tft.setTextSize(2);
    tft.println("  SETTING");
    tft.setTextSize(1);
    tft.drawLine(0,20,tft.height(),20,ST7735_BLACK);
    tft.setCursor(0, 30);
  }
  tft.setTextColor(tx1HexValue);
  tft.print("->  ");
  tft.setTextColor(tx2HexValue);
  tft.println(ipAddrDis);
}
void penModeMeaning1() {
  tft.fillScreen(ST7735_CYAN);
  tft.setCursor(0, 3);
  tft.setTextColor(ST7735_RED);
  tft.setTextSize(2);
  tft.println("  MEANING");
  tft.setTextSize(1);
  tft.drawLine(0,20,tft.height(),20,ST7735_BLACK);
  tft.setCursor(0, 30);
}
void penModeCalculation2() {
  tft.fillScreen(ST7735_YELLOW);
  tft.setCursor(0, 3);
  tft.setTextColor(ST7735_RED);
  tft.setTextSize(2);
  tft.println(" CALCULATE");
  tft.setTextSize(1);
  tft.drawLine(0,20,tft.height(),20,ST7735_BLACK);
  tft.setCursor(0, 30);
}
void penModeGraph3() {
  tft.fillScreen(ST7735_GREEN);
  tft.setCursor(0, 3);
  tft.setTextColor(ST7735_MAGENTA);
  tft.setTextSize(2);
  tft.println("   GRAPH");
  tft.setTextSize(1);
  tft.drawLine(0,20,tft.height(),20,ST7735_BLACK);
  tft.setCursor(0, 30);
}



void tftSerialCPrintln(String abc,int m) {
  unsigned long bgHexValue = ST77XX_BLACK;
  unsigned long tx1HexValue = ST77XX_CYAN;
  unsigned long tx2HexValue = ST77XX_WHITE;

  if (penMode == -1) {
    bgHexValue = ST77XX_BLACK;
    tx1HexValue = ST77XX_CYAN;
    tx2HexValue = ST77XX_WHITE;
    if (tft.getCursorY() >= tft.width() || m==1) {
    tft.fillScreen(bgHexValue);
    tft.setCursor(0, 0);
  }
  } else if (penMode == 0) {
    bgHexValue = ST7735_RED;
    tx1HexValue = ST7735_CYAN ;
    tx2HexValue = ST7735_YELLOW ;
    if (tft.getCursorY() >= tft.width() || m==1) {
      tft.fillScreen(bgHexValue);
      tft.setCursor(0, 3);
      tft.setTextColor(tx2HexValue);
      tft.setTextSize(2);
      tft.println("  SETTING");
      tft.setTextSize(1);
      tft.drawLine(0,20,tft.height(),20,ST7735_BLACK);
      tft.setCursor(0, 30);
    }
  } else if (penMode == 1) {
    bgHexValue = ST7735_CYAN;
    tx1HexValue = ST7735_RED ;
    tx2HexValue = ST7735_BLACK ;
    if (tft.getCursorY() >= tft.width() || m==1) {
      tft.fillScreen(bgHexValue);
      tft.setCursor(0, 3);
      tft.setTextColor(tx2HexValue);
      tft.setTextSize(2);
      tft.println("  MEANING");
      tft.setTextSize(1);
      tft.drawLine(0,20,tft.height(),20,ST7735_BLACK);
      tft.setCursor(0, 30);
    }
  } else if (penMode == 2) {
    bgHexValue = ST7735_YELLOW ;
    tx1HexValue = ST7735_RED;
    tx2HexValue = ST7735_BLACK;
    if (tft.getCursorY() >= tft.width() || m==1) {
      tft.fillScreen(bgHexValue);
      tft.setCursor(0, 3);
      tft.setTextColor(tx2HexValue);
      tft.setTextSize(2);
      tft.println(" CALCULATE");
      tft.setTextSize(1);
      tft.drawLine(0,20,tft.height(),20,ST7735_BLACK);
      tft.setCursor(0, 30);
    }
  } else if (penMode == 3) {
    bgHexValue = ST7735_GREEN ;
    tx1HexValue = ST7735_MAGENTA ;
    tx2HexValue = ST7735_BLACK ;
    if (tft.getCursorY() >= tft.width() || m==1) {
      tft.fillScreen(bgHexValue);
      tft.setCursor(0, 3);
      tft.setTextColor(tx2HexValue);
      tft.setTextSize(2);
      tft.println("   GRAPH");
      tft.setTextSize(1);
      tft.drawLine(0,20,tft.height(),20,ST7735_BLACK);
      tft.setCursor(0, 30);
    }
  }

  if(m==0){
    tft.setTextColor(tx2HexValue);
    tft.print("-> ");
    tft.setTextColor(tx2HexValue);
    tft.println(abc);

  }
  else if(m==1){
    tft.setTextColor(tx1HexValue);
    tft.println(" "+abc);
    tft.println();
  }
}


void longButtonPressed() {
  if (penMode >= 1) {
    capturePhotoSaveSpiffs();
  }
}


