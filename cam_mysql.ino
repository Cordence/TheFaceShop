/*
ESP32-CAM Save a captured photo(Base64) to MySQL. 
Author : ChungYi Fu (Kaohsiung, Taiwan)  2019-9-28 19:00
https://www.facebook.com/francefu
Library
https://github.com/ChuckBell/MySQL_Connector_Arduino
*/

const char* ssid = "U+Net3F5A";
const char* password = "1CB2019693";

IPAddress server_addr(3,21,37,105);
int port = 3306;
char dbUser[] = "me";
char dbPassword[] = "1234";
String databaseName = "skin";
String tableName = "images";
String columnName = "base64";   //Data Type: TEXT, MEDIUMTEXT...

#include <WiFi.h>
#include <MySQL_Connection.h>
#include <MySQL_Cursor.h>
#include <time.h>
WiFiClient client;
MySQL_Connection conn((Client *)&client);  
  
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "Base64.h"

#include "esp_camera.h"

int timezone = 9;
int dst = 0;


// WARNING!!! Make sure that you have either selected ESP32 Wrover Module,
//            or another board which has PSRAM enabled

//CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

void setup() { 
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  
  Serial.println("");
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);

  long int StartTime=millis();
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      if ((StartTime+10000) < millis()) break;
  } 

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(""); 
    Serial.print("Camera Ready! Use 'http://");
    Serial.print(WiFi.localIP());
    Serial.println("' to connect");
    Serial.println("");
  }
  else {
    Serial.println("Connection failed");
    delay(1000);
    ESP.restart();    
    return;
  }
   
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
  //init with high specs to pre-allocate larger buffers
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;  //0-63 lower number means higher quality
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;  //0-63 lower number means higher quality
    config.fb_count = 1;
  }
  
  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    delay(1000);
    ESP.restart();
  }

  //drop down frame size for higher initial frame rate
  sensor_t * s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_VGA);  // VGA|CIF|QVGA|HQVGA|QQVGA
  
  configTime(9 * 3600, 0, "kr.pool.ntp.org", "time.nist.gov");
  Serial.println("\nWaiting for time");
  while (!time(nullptr)) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("");
  
  //SaveCapaturedPhoto2mySQL();
} 
 
void loop() { 
  SaveCapaturedPhoto2mySQL();
  delay(10000);
}

void SaveCapaturedPhoto2mySQL() {
  Serial.println("Connect to mySQL");

  if (conn.connect(server_addr, port, dbUser, dbPassword)) {
    Serial.println("Connection successful");
    
    camera_fb_t * fb = NULL;
    fb = esp_camera_fb_get();  
    if(!fb) {
      Serial.println("Camera capture failed");
      delay(1000);
      ESP.restart();
      return;
    }
  
    char *input = (char *)fb->buf;
    char output[base64_enc_len(3)];
    String imageFile = "data:image/jpeg;base64,";
    for (int i=0;i<fb->len;i++) {
      base64_encode(output, (input++), 3);
      if (i%3==0) imageFile += urlencode(String(output));
    }

    esp_camera_fb_return(fb);

    Serial.println("Image size = " + String(imageFile.length()));   

    MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
    String tm = getTime();
    String INSERT_SQL = "INSERT INTO "+databaseName+"."+tableName+" (time, "+columnName+") VALUES ('"+ tm + "', '"+imageFile+"')";
    cur_mem->execute(INSERT_SQL.c_str());  // How to modify the code to upload a big image? (UXGA|SXGA|XGA|SVG)
    delete cur_mem;
    
    Serial.println("Data stored!");
    Serial.println("");
  }
  else {
    ledcAttachPin(4, 3);
    ledcSetup(3, 5000, 8);
    ledcWrite(3,10);
    delay(200);
    ledcWrite(3,0);
    delay(200);    
    ledcDetachPin(3);
          
    Serial.println("Connected to " + String(server_addr) + " failed.");
  }
  client.stop();
}

String getTime(void) {
  String a[6];
  time_t now = time(nullptr);
  struct tm *timeinfo;
  timeinfo = localtime(&now);

  a[0] = String(timeinfo->tm_year + 1900, DEC);
  a[1] = String(timeinfo->tm_mon + 1, DEC);
  a[2] = String(timeinfo->tm_mday, DEC);
  a[3] = String(timeinfo->tm_hour, DEC);  
  a[4] = String(timeinfo->tm_min, DEC); 
  a[5] = String(timeinfo->tm_sec, DEC);
  
  if(timeinfo->tm_mon < 10)     
    a[1]="0"+a[1];
  if(timeinfo->tm_mday < 10)     
    a[2]="0"+a[2];
  if(timeinfo->tm_hour < 10)     
    a[3]="0"+a[3];
  if(timeinfo->tm_min < 10)      
    a[4]="0"+a[4];
  if(timeinfo->tm_sec < 10)      
    a[5]="0"+a[5];
  String tm = a[0] +'-' + a[1] + '-' + a[2] + ' ' + a[3] + ':' + a[4] + ':' + a[5];
  Serial.println(tm);

  return tm;
}

//https://github.com/zenmanenergy/ESP8266-Arduino-Examples/
String urlencode(String str)
{
    String encodedString="";
    char c;
    char code0;
    char code1;
    char code2;
    for (int i =0; i < str.length(); i++){
      c=str.charAt(i);
      if (c == ' '){
        encodedString+= '+';
      } else if (isalnum(c)){
        encodedString+=c;
      } else{
        code1=(c & 0xf)+'0';
        if ((c & 0xf) >9){
            code1=(c & 0xf) - 10 + 'A';
        }
        c=(c>>4)&0xf;
        code0=c+'0';
        if (c > 9){
            code0=c - 10 + 'A';
        }
        code2='\0';
        encodedString+='%';
        encodedString+=code0;
        encodedString+=code1;
        //encodedString+=code2;
      }
      yield();
    }
    return encodedString;
}
