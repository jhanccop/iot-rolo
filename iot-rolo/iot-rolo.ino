/*
  Estacion GRPS LCD pressure station v.1
*/

/* ====================== GPRS CONFIG ======================== */

/* settings grps apn */
const char apn[]  = "claro.pe";
const char gprsUser[] = "claro@datos";
const char gprsPass[] = "claro";

/* pin sim */
const char simPIN[]   = "1111";

//#define MODEM_RST       5
//#define MODEM_POWER_ON  1
#define MODEM_TX        16
#define MODEM_RX        17

/* Set serial for AT commands (to SIM800 module) */
#define SerialAT Serial1

/* Configure TinyGSM library */
#define TINY_GSM_MODEM_SIM800      // Modem is SIM800
#define TINY_GSM_RX_BUFFER   1024  // Set RX buffer to 1Kb

#include <TinyGsmClient.h>

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

TinyGsmClient client(modem);

/* ====================== MQTT CONFIG ======================== */

#include <PubSubClient.h>

PubSubClient mqtt(client);

/* settings MQTT */
const char *broker = "esp-jhanccop.ga";
const int mqtt_port = 1883;
const char *mqtt_user = "web_client";
const char *mqtt_pass = "080076C";

const String serial_number = "202003";

const char* topicLed = "GsmClientTest/led";
const char* topicInit = "GsmClientTest/init";
const char* topicLedStatus = "GsmClientTest/ledStatus";

long lastMsg = 0;
char msg[50];
char msg_c[50];

unsigned long previousMillis = 0; // init millis wait to mqtt callback
const long interval = 5000; // secont wait to mqtt callback

//*****************************
//*** DECLARACION FUNCIONES ***
//*****************************
boolean setup_grps();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void reconnect();

/* ====================== SLEEP MODE CONFIG ======================== */

#define uS_TO_S_FACTOR 1000000     /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP 100        /* Time ESP32 will go to sleep (in seconds) */

#define Threshold 40 /* Greater the value, more the sensitivity */

RTC_DATA_ATTR int readingID = 0;
RTC_DATA_ATTR int id_file = 0; // id file to save example data1.txt this increase only readingID equal zero
touch_pad_t touchPin;

/* ====================== LCD CONFIG ======================== */
#include <Arduino.h>
#include <U8g2lib.h>

U8G2_KS0108_ERM19264_F u8g2(U8G2_R0, 33, 25, 26, 27, 14, 12, 13, 15, /*enable=*/ 32, /*rs dc=*/ 2, /*cs0=*/ 21, /*cs1=*/ 22, /*cs2=*/ 0, /* reset=*/  U8X8_PIN_NONE);   // Set R/W to low!

/* ====================== SD CONFIG ======================== */
#include "FS.h"
#include "SD.h"
//#include <SPI.h>
#include "SPI.h"

#define SD_CS 5

int num_dir = 0;

char path_file[25]; // name of file

/* ====================== SEPARADOR ======================== */
#include <Average.h>
#include <Separador.h>

Average<float> t(10000);
Average<float> x(10000);
Average<float> y(10000);

Average<float> val_max(2);

Separador s;

int line = 0;

/* ====================== VARIABLES ======================== */
float sensor1 = 0;
float sensor2 = 0;
float hour = 0;
String dateTime = "";

int cont1 = 0;
long refresh = 5000;
int LED_PIN = 13;

int ledStatus = LOW;

long lastReconnectAttempt = 0;

/* ************************* u8g2 PREPARE INIT ************************** */
void u8g2_prepare(void) {
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setFontRefHeightExtendedText();
  u8g2.setDrawColor(1);
  u8g2.setFontPosTop();
  u8g2.setFontDirection(0);
}

/* ************************* SETUP GRPS ************************** */
boolean setup_grps() {

  // Set GSM module baud rate and UART pins
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000);

  // Restart takes quite some time
  print_msg("Init modem");
  status_sim(0x2610); // sim init
  //modem.restart();

  String modemInfo = modem.getModemInfo();
  print_msg(modemInfo);
  status_sim(0x21b9);

  if (modemInfo == "") {
    print_msg("modem fail");
    return false;
  }

  print_msg("network");
  if (!modem.waitForNetwork(240000L)) {
    print_msg("nt fail");
    status_sim(0x2612); //sim fail
    return false;
  }

  if (modem.isNetworkConnected()) {
    print_msg("connected");
    print_msg("claro");
    status_sim(0x2611); // sim ok
  }
  else {
    return false;
  }

  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    print_msg("grps fail");
    status_sim(0x2718);
    return false;
  } else {
    status_sim(0x2714); // sim  grps ok
    return true;
  }

}

/* ************************** MEASUREMENT SENSORS ****************************** */
void read_sensors() {
  int temp1 = 0;
  int temp2 = 0;

  for (int i = 0; i < 5; i++) {
    temp1 += analogRead(34);
    delay(20);
    temp2 += analogRead(39);
    delay(20);
  }
  hour = readingID * 0.016667;
  print_msg(String(hour, 6));
  delay(500);
  sensor1 = temp1 * 0.1; // pressure 1
  sensor2 = temp2 * 0.1; // pressure 2

}


/* ************************* LOOP ************************** */
void loop() {

}

/* ************************* MQTT CALLBACK ************************** */
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String incoming = "";

  for (int i = 0; i < length; i++) {
    incoming += (char)payload[i];
  }
  dateTime = incoming;
  print_datetime(incoming);

  read_sensors();
  
  setup_SDcard();
  char path_file[25];
  String path_save_s = "/data" + String(id_file)  + ".txt";
  path_save_s.toCharArray(path_file, 50);
  
  String dataMessage = String(readingID) + "," + String(hour, 6) + "," + String(sensor1, 2) + "," + String(sensor1, 2) + "," + String(incoming) + "\r\n";
  logSDCard(path_file, dataMessage);

  print_msg("mqtt save");

}

/* ************************* RECONNECT ************************** */
void reconnect() {

  while (!mqtt.connected()) {
    print_msg("Mqtt....");
    // Creamos un cliente ID
    String clientId = "202003_";
    clientId += String(random(0xffff), HEX);
    // Intentamos conectar
    if (mqtt.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
      print_msg("mqtt conected");

      // Nos suscribimos
      char topic[25];
      String topic_aux = serial_number + "/command";
      topic_aux.toCharArray(topic, 50);
      mqtt.subscribe(topic);

    } else {
      print_msg("fail :( ");
      mqtt.state();
      print_msg("retry...");

      delay(2000);
    }
  }
}

/* ************************* PRINT VALUES ************************** */
void print_values(float value1, float value2) {

  // VALUE PRESSURE
  value1 = value1 * 0.1;
  value2 = value2 * 0.1;

  uint8_t x_8 = 16;  // Update area left position (in tiles)
  uint8_t y_8 = 0;  // Update area upper position (distance from top in tiles)
  uint8_t width_8 = 8; // ancho
  uint8_t height_8 = 4; // alto

  u8g2.clearBuffer();

  // BOX PRESSURE
  u8g2.drawRFrame(128, 0, 64, 32, 1);

  // LABEL PRESSURE
  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.drawStr( 130, 2, "P1*");
  u8g2.drawStr( 130, 16, "P2+");

  // VALUE PRESSURE
  u8g2.setFont(u8g2_font_ncenB10_tf);
  u8g2.setCursor( 147, 2);
  u8g2.print(String(value1, 1));

  u8g2.setCursor( 147, 16);
  u8g2.print(String(value2, 1));

  //u8g2.drawStr(x_pos, y_pos, val);
  u8g2.updateDisplayArea(x_8, y_8, width_8, height_8);
}

/* ************************* PRINT DATETIME ************************** */
void print_datetime(String datetime) {

  uint8_t x_8 = 16;  // Update area left position (in tiles)
  uint8_t y_8 = 4;  // Update area upper position (distance from top in tiles)
  uint8_t width_8 = 8; // ancho
  uint8_t height_8 = 1; // alto

  u8g2.clearBuffer();

  // VALUE PRESSURE
  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.setCursor(134, 32);
  u8g2.print(datetime);

  //u8g2.drawStr(x_pos, y_pos, val);
  u8g2.updateDisplayArea(x_8, y_8, width_8, height_8);
}

/* ************************* PRINT MESSAGE ************************** */
void print_msg(String msg) {

  uint8_t x_8 = 16;  // Update area left position (in tiles)
  uint8_t y_8 = 5;  // Update area upper position (distance from top in tiles)
  uint8_t width_8 = 8; // ancho
  uint8_t height_8 = 1; // alto

  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_helvB08_tr);
  u8g2.setCursor(128, 39);
  u8g2.print(msg);
  u8g2.updateDisplayArea(x_8, y_8, width_8, height_8);
  delay(500);
}

/* ************************* STATUS PANEL ************************** */
void status_panel(int panel) {

  uint8_t x_8 = 16;  // Update area left position (in tiles)
  uint8_t y_8 = 6;  // Update area upper position (distance from top in tiles)
  uint8_t width_8 = 1; // ancho
  uint8_t height_8 = 2; // alto

  u8g2.clearBuffer();

  // STATUS PANEL
  u8g2.setFont(u8g2_font_unifont_t_symbols);
  u8g2.drawGlyph(128, 50, panel);

  u8g2.updateDisplayArea(x_8, y_8, width_8, height_8);
}

/* ************************* STATUS BATTERY ************************** */
void status_battery(int battery) {

  uint8_t x_8 = 17;  // Update area left position (in tiles)
  uint8_t y_8 = 6;  // Update area upper position (distance from top in tiles)
  uint8_t width_8 = 1; // ancho
  uint8_t height_8 = 2; // alto

  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_unifont_t_symbols);
  u8g2.drawGlyph(136, 50, battery);
  // u8g2.drawRFrame(148, 50, 8, 10, 1);

  u8g2.updateDisplayArea(x_8, y_8, width_8, height_8);
}

/* ************************* STATUS SIM800 ************************** */
void status_sim(int sim) {

  uint8_t x_8 = 22;  // Update area left position (in tiles)
  uint8_t y_8 = 6;  // Update area upper position (distance from top in tiles)
  uint8_t width_8 = 2; // ancho
  uint8_t height_8 = 2; // alto

  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_unifont_t_symbols);
  u8g2.drawGlyph(176, 50, sim);

  u8g2.updateDisplayArea(x_8, y_8, width_8, height_8);
}

/* ************************* STATUS MQTT CONNECTION ************************** */
void status_mqtt(int mqtt_s) {

  uint8_t x_8 = 20;  // Update area left position (in tiles)
  uint8_t y_8 = 6;  // Update area upper position (distance from top in tiles)
  uint8_t width_8 = 2; // ancho
  uint8_t height_8 = 2; // alto

  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_unifont_t_symbols);
  u8g2.drawGlyph(160, 50, mqtt_s);
  u8g2.updateDisplayArea(x_8, y_8, width_8, height_8);
}

/* ************************* PLOTTER ************************** */
void plotter() {

  uint8_t x_8 = 0;  // Update area left position (in tiles)
  uint8_t y_8 = 0;  // Update area upper position (distance from top in tiles)
  uint8_t width_8 = 16; // ancho
  uint8_t height_8 = 8; // alto

  u8g2.clearBuffer();

  u8g2.drawRFrame(8, 0, 113, 57, 0);

  // marker horizontal
  for ( int  i = 3; i <= 13; i = i + 2) {
    u8g2.drawPixel(i * 8, 55);
  }

  // marker vertical
  for ( int  j = 1; j <= 5; j = j + 2) {
    u8g2.drawPixel(9, j * 8);
  }

  // INDEX LIST
  int t_pmax = 0;
  int t_pmin = 0;

  int x_pmax = 0;
  int x_pmin = 0;

  int y_pmax = 0;
  int y_pmin = 0;

  // VALUE MAXIME TO LIST
  float t_max = t.maximum(&t_pmax);
  float x_max = x.maximum(&x_pmax);
  float y_max = y.maximum(&y_pmax);

  Serial.println(line);
  Serial.println(t_max);
  Serial.println(x_max);
  Serial.println(y_max);

  float t_min = t.minimum(&t_pmin);
  float x_min = x.minimum(&x_pmin);
  float y_min = y.minimum(&y_pmin);

  // PIXEL INIT
  int To = 8;
  int Yo1 = 57;
  int Yo2 = 57;

  val_max.push(x_max);
  val_max.push(y_max);

  int value_max = val_max.maximum();

  for (int i = 0; i < line - 1; i++) {
    Serial.println("----------");
    Serial.println("t" + String(i) + "-> " + String(t.get(i), 4));
    Serial.println("x" + String(i) + "-> " + String(x.get(i)));
    Serial.println("y" + String(i) + "-> " + String(y.get(i)));
  }

  if (line <= 104) {
    for (int i = 0; i < line - 1; i++) {
      float Tp = 104 * t.get(i) / t_max + 8;
      float Y1p = -50 * x.get(i) / value_max + 57;
      float Y2p = -50 * y.get(i) / value_max + 57;
      u8g2.drawLine(To, Yo1, Tp, Y1p);
      u8g2.drawLine(To, Yo2, Tp, Y2p);

      To = Tp;
      Yo1 = Y1p;
      Yo2 = Y2p;
    }
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawStr( To, Yo1 - 4, "*");
    u8g2.drawStr( To, Yo2 - 4, "+");
    u8g2.setCursor(5, 57);
    u8g2.print("0");
    u8g2.setCursor(To - 5, 57);
    u8g2.print(t_max);

    u8g2.setCursor(0, 3);
    u8g2.print(value_max);
  } else {
    float temp_factor = line / 103;
    int factor = ceil(temp_factor);
    for (int i = 0; i < line - 1; i = i + factor) {
      float Tp = 104 * t.get(i) / t_max + 8;
      float Y1p = -50 * x.get(i) / value_max + 57;
      float Y2p = -50 * y.get(i) / value_max + 57;
      u8g2.drawLine(To, Yo1, Tp, Y1p);
      u8g2.drawLine(To, Yo2, Tp, Y2p);

      To = Tp;
      Yo1 = Y1p;
      Yo2 = Y2p;
    }

    float Tp = 104 * t.get(line - 2) / t_max + 8;
    float Y1p = -50 * x.get(line - 2) / value_max + 57;
    float Y2p = -50 * y.get(line - 2) / value_max + 57;
    u8g2.drawLine(To, Yo1, Tp, Y1p);
    u8g2.drawLine(To, Yo2, Tp, Y2p);
    To = Tp;
    Yo1 = Y1p;
    Yo2 = Y2p;

    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawStr( To, Yo1 - 4, "*");
    u8g2.drawStr( To, Yo2 - 4, "+");
    u8g2.setCursor(5, 57);
    u8g2.print("0");
    u8g2.setCursor(To - 5, 57);
    u8g2.print(t_max);

    u8g2.setCursor(0, 3);
    u8g2.print(value_max);

  }

  u8g2.updateDisplayArea(x_8, y_8, width_8, height_8);
}

/* ************************* SETUP SD_CARD ************************** */
void setup_SDcard() {

  SD.begin(SD_CS);
  if (!SD.begin(SD_CS)) {
    print_msg("Card Failed");
    return;
  }

  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    print_msg("No SD card");
    return;
  }

  String sd_type = "";

  if (cardType == CARD_MMC) {
    sd_type = "MMC";
  } else if (cardType == CARD_SD) {
    sd_type = "SDSC";
  } else if (cardType == CARD_SDHC) {
    sd_type = "SDHC";
  } else {
    sd_type = "UNKNOWN";
  }

  print_msg(sd_type);
}

/* ************************* WRITE SD_CARD ************************** */
void writeFile(fs::FS &fs, const char * path, const char * message) {
  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    print_msg("SD error");
    return;
  }
  if (file.print(message)) {
    print_msg("File written");
  } else {
    print_msg("Write failed");
  }
  file.close();
}

/* ************************* LOG FILE SD_CARD ************************** */
void logSDCard(const char * path, String text) {
  //dataMessage = String(readingID) + "," + String(0.1) + "," + String(10.3) + "," + String(4.5) + "\r\n";
  //Serial.print("Save data: ");
  //Serial.println(dataMessage);
  appendFile(SD, path, text.c_str());
}

/* ************************* APPENDFILE SD_CARD ************************** */
void appendFile(fs::FS &fs, const char * path, const char * message) {
  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    print_msg("SD error");
    return;
  }
  if (file.print(message)) {
    String temp_text = "A" + String(path);
    print_msg(temp_text);
  } else {
    print_msg("Save failed");
  }
  file.close();
}

/* ************************* READ SD_CARD ************************** */
void readFile(fs::FS &fs, const char * path) {
  String temp_text = "R" + String(path);
  print_msg(temp_text);

  File file = fs.open(path);
  if (!file) {
    print_msg("Failed open");
    return;
  }

  char chr;
  String arry = "";

  while (file.available()) {
    chr = file.read();
    arry += chr;

    if (chr == 10) {
      if (line > 0) {
        String t_temp = s.separa(arry, ',', 1);
        String x_temp = s.separa(arry, ',', 2);
        String y_temp = s.separa(arry, ',', 3);
        dateTime = s.separa(arry, ',', 4);
        /*
          Serial.println("----------");
          Serial.println("t:" + t_temp);
          Serial.println("x:" + x_temp);
          Serial.println("y:" + y_temp);
          Serial.println("dt:" + dateTime);
        */
        t.push(t_temp.toFloat());
        x.push(x_temp.toFloat());
        y.push(y_temp.toFloat());
      }
      line++;
      arry = "";
    }
  }
  file.close();
}

/* ************************* LIST DIR SD_CARD ************************** */
int listDir(fs::FS &fs, const char * dirname, uint8_t levels) {
  int num = 0;
  File root = fs.open(dirname);
  if (!root) {
    print_msg("Failed SD");
    return num;
  }
  if (!root.isDirectory()) {
    print_msg("Not dir");
    return num;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      print_msg(String(file.name()));
      if (levels) {
        listDir(fs, file.name(), levels - 1);
      }
    } else {
      num++;
    }
    file = root.openNextFile();
  }
  String temp_text = "files: " + String(num);
  print_msg(temp_text);
  return num;
}

/* ************************* PRINT WAKEUP REASON ESP32 ************************** */
void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_TIMER : print_msg("TIMER"); delay(500); wakeup_timer(); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : print_msg("TOUCHPAD"); wakeup_touchpad();; break;
    case ESP_SLEEP_WAKEUP_ULP : print_msg("ULP"); break;
    default : print_msg("OTHER"); break;
  }
}

void print_wakeup_touchpad() {
  touch_pad_t pin;

  touchPin = esp_sleep_get_touchpad_wakeup_status();

  wakeup_touchpad();
}

/* ************************* TOUCHPAD ************************** */
void wakeup_touchpad() {

  /* ======= LOAD DATA FROM SD ====== */
  setup_SDcard();
  char path_file[25];
  String path_save_s = "/data" + String(id_file)  + ".txt";
  path_save_s.toCharArray(path_file, 50);

  readFile(SD, path_file);

  /* ======= PLOT DATA ====== */
  plotter();

  /* ======= PRINT VALUES ====== */
  print_values(x.get(line - 2), y.get(line - 2));

  print_datetime(dateTime);
  delay(1000);

}

/* ************************* TIMER ************************** */
void wakeup_timer() {

  setup_SDcard();
  char path_file[25];
  String path_save_s = "/data" + String(id_file)  + ".txt";
  path_save_s.toCharArray(path_file, 50);

  boolean grps_ok = setup_grps() ;

  if (grps_ok) {
    read_sensors();
    get_mqtt();
    if (!mqtt.connected()) {
      reconnect();
    }
    //mqtt.loop();

  } else {
    logSDCard_simple(path_file); // save data simple, only sensors data
  }

  /* ======= LOAD DATA FROM SD ====== */


  readFile(SD, path_file);

  /* ======= PRINT VALUES ====== */
  print_values(x.get(line - 2), y.get(line - 2));
  print_datetime(dateTime);

  /* ======= PLOT DATA ====== */
  plotter();

  /* ======= ADD ID======= */
  ++readingID;

  delay(1000);
}

/* ********************************************************** */
/* ************************* SETUP ************************** */
/* ********************************************************** */

void setup() {
  // read init id test, if readingID equal to 0, created file
  Serial.begin(115200);
  u8g2.begin();
  u8g2_prepare();

  String temp_text = "R_ID: " + String(readingID);
  print_msg(temp_text);

  if (readingID == 0) {
    setup_test();
    readingID++;
  } else {
    //String path_save_s = "/data" + String(id_file)  + ".txt";
    //path_save_s.toCharArray(path_file, 50);

    print_wakeup_reason();
    //print_wakeup_touchpad();
  }

  //esp_light_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

  //Setup interrupt on Touch Pad 0 (GPIO4)
  touchAttachInterrupt(T0, callback, Threshold);

  //Configure Touchpad as wakeup source
  //esp_light_enable_touchpad_wakeup();
  esp_sleep_enable_touchpad_wakeup();

  delay(500);

  print_msg("SLEEP");
  modem.restart();
  //esp_light_sleep_start();
  esp_deep_sleep_start();

}

void callback() {
  //placeholder callback function
}

void setup_test() {
  /* ======= SCREEN START ====== */
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_ncenB18_tr);
  u8g2.drawStr( 20, 10, "esp-jhanccop");

  u8g2.drawStr( 50, 40, "IIot");
  u8g2.sendBuffer();

  delay(1000);

  setup_SDcard();

  /* ======= LIST DIR ====== */
  id_file = listDir(SD, "/", 0);

  char path_create[25];
  String path_create_s = "/data" + String(id_file)  + ".txt";
  path_create_s.toCharArray(path_create, 50);

  /* ======= Create a file on the SD card ====== */
  File file = SD.open(path_create);
  if (!file) {
    String temp_text = "C:" + String(path_create);
    print_msg(temp_text);
    writeFile(SD, path_create, "Id,Hour,Pressure1,Pressure2,DateTime\r\n");
  }
  else {
    print_msg(String(path_create));
  }
  file.close();

  boolean grps_ok = setup_grps() ;

  if (grps_ok) {
    read_sensors();
    get_mqtt();
    if (!mqtt.connected()) {
      reconnect();
    }


  } else {
    logSDCard_simple(path_create); // save data simple, only sensors data
  }
  print_values(sensor1, sensor2);
  print_datetime(dateTime);

}

/* ************************* LOG SIMPLE SAVE SDCARD ************************** */
void logSDCard_simple(const char * path) {
  read_sensors();
  String dataMessage = String(readingID) + "," + String(hour, 6) + "," + String(sensor1, 2) + "," + String(sensor2, 2) + "," + "--\r\n";

  logSDCard(path, dataMessage);
}

/* ************************* SETUP AND GET DATA MQTT ************************** */
void get_mqtt() {
  // MQTT Broker setup
  mqtt.setServer(broker, mqtt_port);
  mqtt.setCallback(mqttCallback);
  print_msg("mqtt ok! ");
  status_mqtt(0x2611);

  String clientId = "202003_";
  clientId += String(random(0xffff), HEX);

  if (mqtt.connect(clientId.c_str(), mqtt_user, mqtt_pass))
  {
    char topic_s[25];
    String topic_t = serial_number + "/command";
    topic_t.toCharArray(topic_s, 50);
    mqtt.subscribe(topic_s);

    // SEND DATA MQTT
    String to_send = String(sensor1) + "," + String(sensor2) + "," + String(readingID);
    to_send.toCharArray(msg, 50);

    char topic[25];
    String topic_aux = serial_number + "/data";
    topic_aux.toCharArray(topic, 50);

    mqtt.publish(topic, msg);

  }


  print_msg("start listening");

  int cont = 0;
  
  while (cont<3000) {
    mqtt.loop();
    /*unsigned long currentMillis = millis();
    if (currentMillis - previousMillis > interval) {
      previousMillis = currentMillis;
      print_msg("end listening");
      break;
    }*/
    cont++;
    delay(1);
  }
  print_msg("end listening");

}
