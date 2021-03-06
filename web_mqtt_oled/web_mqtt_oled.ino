#include <Arduino.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <CapacitiveSensor.h>

#include <PersWiFiManager.h>
#include <ArduinoJson.h>
#include <EasySSDP.h>
#include <SPIFFSReadServer.h>
#include <DNSServer.h>
#include <FS.h>
#define DEVICE_NAME "tugdutrucdisplay"
#define HISTORY_FILE "/history.json"
#define CONFIG_FILE "/config.json"

//server objects
SPIFFSReadServer server(80);
DNSServer dnsServer;
PersWiFiManager persWM(server, dnsServer);
/*
   Your WiFi config here
*/
char ssid[] = "tugwifi";    // your network SSID (name)
char pass[] = "7tugdual7";  // your network password
String temp_out;
String temp_in;
String temp;
WiFiUDP ntpUDP;
int contrast = 0;
int touch_count = 0;
int menu = 0;
const char* mqtt_server = "tugdutrucmqttserver";
const char* mqtt_username = "";
const char* mqtt_password = "";
const char* mqtt_topic = "sensors/test/temperature";
// touch sensor
CapacitiveSensor   cs_4_5 = CapacitiveSensor(14,13);        // 10 megohm resistor between pins 4 & 6, pin 6 is sensor pin, add wire, foil

// By default 'time.nist.gov' is used with 60 seconds update interval and
// no offset
//NTPClient timeClient(ntpUDP);

// You can specify the time server pool and the offset, (in seconds)
// additionaly you can specify the update interval (in milliseconds).
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);
unsigned int mqttPort = 1883;       // the standard MQTT broker port
unsigned int max_subscriptions = 30;
unsigned int max_retained_topics = 30;

//U8G2_ST7920_128X64_1_SW_SPI u8g2(U8G2_R0, 13, 11, 10, 8);
U8G2_SSD1306_64X48_ER_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);


long intervalHist = 30000;
int     sizeHist = 840 ;        // Taille historique (7h x 12pts) - History size = ~3j
unsigned long previousMillis = intervalHist;

long intervalDisplayRefersh = 30000;
unsigned long previousMillisDisp = intervalDisplayRefersh;

StaticJsonBuffer<1000> jsonBuffer;      // Buffer static contenant le JSON courant - Current JSON static buffer
JsonObject& root = jsonBuffer.createObject();
JsonArray& timestamp = root.createNestedArray("timestamp");
JsonArray& hist_t = root.createNestedArray("temp");

StaticJsonBuffer<1000> jsonBufferConf;      // Buffer static contenant le JSON courant - Current JSON static buffer
JsonObject& conf = jsonBufferConf.createObject();
// JsonArray& param = conf.createNestedArray("param");
// JsonArray& value = conf.createNestedArray("value");

char json[100];


void data_callback(char* topic, byte* data, unsigned int length) {
//void data_callback(uint32_t *client /* we can ignore this */, const char* topic, uint32_t topic_len, const char *data, uint32_t lengh) {
//  char topic_str[topic_len + 1];
//  os_memcpy(topic_str, topic, topic_len);
//  topic_str[topic_len] = '\0';

  char data_str[length + 1];
  os_memcpy(data_str, data, length);
  data_str[length] = '\0';

  Serial.print("received topic '");
  Serial.print(topic);
  Serial.print("' with data '");
  Serial.print(data_str);
  Serial.println("'");
  temp = String(data_str[0])+String(data_str[1])+String(data_str[2])+String(data_str[3]);
  if (strcmp( topic, "sensor/192.168.2.115/2c:3a:e8:4e:3c:13/temp")== 0) {
     temp_out = temp;
   }
   if (strcmp( topic, "sensor/192.168.2.112/5c:cf:7f:34:37:48/temp")== 0) {
     temp_in = temp;
   }
   if (strcmp( topic, "lum")== 0) {
	   touch_count = 6;
      }
  display_menu(menu);
}
WiFiClient espClient;
PubSubClient client(mqtt_server,1883,data_callback,espClient);

String getHourMinute() {
  timeClient.update();
  unsigned long rawTime = timeClient.getEpochTime();
  unsigned long hours = (rawTime % 86400L) / 3600;
  String hoursStr = hours < 10 ? "0" + String(hours) : String(hours);

  unsigned long minutes = (rawTime % 3600) / 60;
  String minuteStr = minutes < 10 ? "0" + String(minutes) : String(minutes);

  unsigned long seconds = rawTime % 60;
  String secondStr = seconds < 10 ? "0" + String(seconds) : String(seconds);

  return hoursStr + ":" + minuteStr;
}
void oled_display() {


  String time_now = "";
  time_now = getHourMinute();
  Serial.println(time_now);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_helvR18_tn   );
  u8g2.drawStr(5, 18, time_now.c_str());
  oled_display_small("in", 0, 35);
  u8g2.setFont(u8g2_font_profont12_tn);
  u8g2.drawStr(0, 48, temp_in.c_str());
  oled_display_small("out", 40, 35);
  u8g2.setFont(u8g2_font_profont12_tn);
  u8g2.drawStr(40, 48, temp_out.c_str());
  u8g2.sendBuffer();
}
void oled_display_start(const char* message, int x, int y) {
  u8g2.setFont(u8g2_font_smart_patrol_nbp_tr);
  u8g2.drawStr(x, y, message);
  u8g2.sendBuffer();
}
void oled_display_small(const char* message, int x, int y) {
  u8g2.setFont(u8g2_font_smart_patrol_nbp_tr);
  //u8g2.setFont(u8g2_font_profont12_tn);
  u8g2.drawStr(x, y, message);
  u8g2.sendBuffer();
}
void oled_display_in() {
	u8g2.clearBuffer();
	u8g2.setFont(u8g2_font_smart_patrol_nbp_tr);
	u8g2.drawStr(2, 10, "in");
	u8g2.setFont(u8g2_font_helvB24_tn );
	u8g2.drawStr(2, 40, temp_in.c_str());
	u8g2.sendBuffer();
}
void oled_display_out() {
	u8g2.clearBuffer();
	u8g2.setFont(u8g2_font_smart_patrol_nbp_tr);
	u8g2.drawStr(2, 10, "out");
	u8g2.setFont(u8g2_font_helvB24_tn );
	u8g2.drawStr(2, 40, temp_out.c_str());
	u8g2.sendBuffer();
}

void sendHistory(){
  server.send(200, "application/json", json);   // Envoi l'historique au client Web - Send history data to the web client
}
void loadHistory(){
  File file = SPIFFS.open(HISTORY_FILE, "r");
  if (!file){
    Serial.println("Aucun historique existe - No History Exist");
  } else {
    size_t size = file.size();
    if ( size == 0 ) {
      Serial.println("Fichier historique vide - History file empty !");
    } else {
      std::unique_ptr<char[]> buf (new char[size]);
      file.readBytes(buf.get(), size);
      JsonObject& root = jsonBuffer.parseObject(buf.get());

      if (!root.success()) {
        Serial.println("Impossible de lire le JSON - Impossible to read JSON file");
      } else {
        Serial.println("History loaded");
        //root.prettyPrintTo(Serial);
        root.printTo(json, sizeof(json));
        Serial.println("json loaded");
        Serial.println(json);
      }
    }
    file.close();
  }
}
void saveHistory(){
  File historyFile = SPIFFS.open(HISTORY_FILE, "w");
  root.printTo(historyFile); // Exporte et enregsitre le JSON dans la zone SPIFFS - Export and save JSON object to SPIFFS area
  historyFile.close();
}
void addPtToHist(){
  unsigned long currentMillis = millis();

  if ( currentMillis - previousMillis > intervalHist ) {
    long int tps = timeClient.getEpochTime();
    previousMillis = currentMillis;
    if ( tps > 0 ) {
      timestamp.add(tps);
      hist_t.add(double_with_n_digits(temp_out.toFloat(), 1));

      if ( hist_t.size() > sizeHist ) {
        timestamp.removeAt(0);
        hist_t.removeAt(0);
      }
      delay(100);
      root.printTo(json, sizeof(json));
      saveHistory();
    }
  }
}

void setup(void) {
  u8g2.begin();
  u8g2.setContrast(contrast);
  Serial.begin(115200);
  oled_display_start("Tugdu", 0, 10);
  oled_display_start("r", 0, 20);
  oled_display_start("u", 0, 30);
  oled_display_start("c", 0, 40);


  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);

  //optional code handlers to run everytime wifi is connected...
      persWM.onConnect([]() {
        Serial.print("wifi connected");
        Serial.print(WiFi.localIP());
        EasySSDP::begin(server);
      });
      //...or AP mode is started
      persWM.onAp([](){
        Serial.print("AP MODE");
        Serial.print(persWM.getApSsid());
      });

      //allows serving of files from SPIFFS
        if (!SPIFFS.begin()) {
        Serial.println("SPIFFS Mount failed");        // Problème avec le stockage SPIFFS - Serious problem with SPIFFS
      } else {
        Serial.println("SPIFFS Mount succesfull");
        //loadHistory();
      }
      delay(50);
      //loadConfig();
      //loadHistory();
      //sets network name for AP mode
      persWM.setApCredentials(DEVICE_NAME);
      //persWM.setApCredentials(DEVICE_NAME, "password"); optional password

      //make connecting/disconnecting non-blocking
      persWM.setConnectNonBlock(true);

      //in non-blocking mode, program will continue past this point without waiting
      persWM.begin();

      //handles commands from webpage, sends live data in JSON format
      server.on("/graph_temp.json", sendHistory);
      //server.on("/config.json", configure);

//      server.on("/wifi", []() {
//
//        }); //server.on wifi

        server.on("/temp", []() {

        //build json object of program data
        StaticJsonBuffer<200> jsonBuffer;
        JsonObject &jsonAP = jsonBuffer.createObject();

        String json_now = "{\"temp\":\"" + String(temp_out) + "\",";
        json_now += "\"timestamp\":\"" + timeClient.getFormattedTime(); + "\"}";
        server.send(200, "application/json", json_now);
        addPtToHist();
        //sendHistory();

      }); //server.on temp


      server.begin();


      timeClient.begin();

}

void reconnect() {
  // Loop until we're reconnected
  //while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = DEVICE_NAME;
    //clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      //client.publish("outTopic", "hello world");
      // ... and resubscribe
      client.subscribe("#");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      //delay(5000);
    }
  //}
}

void display_menu(int menu){
	switch (menu) {
	case 0:
		oled_display();
		break;
	case 1:
		oled_display_in();
		break;
	case 2:
		oled_display_out();
		break;
	default:
		oled_display();
	}
}
void loop(void) {
	Serial.print(".");

      long touch =  cs_4_5.capacitiveSensor(30);
      if (touch >=0 ){
    	  touch_count ++;
    	  Serial.print("touch: ");
    	  Serial.println(touch_count);

      }
      else
      {
    	  if (touch_count >=5 ){

    	      	  contrast = contrast+125;
    	      	  if (contrast > 250) contrast = 0;
    	      	  Serial.print("contrast : ");
				  Serial.println(contrast);
    	      	  u8g2.setContrast(contrast);
			}
			else if (touch_count >=1){
				Serial.print("menu : ");
				Serial.println(menu);
			    menu = (menu+1)%3;
			    display_menu(menu);
			}
    	  touch_count = 0;
      }

      unsigned long currentMillis = millis();

        if ( currentMillis - previousMillisDisp > intervalDisplayRefersh ) {
        	previousMillisDisp = currentMillis;

        	display_menu(menu);
        }


   	 if (!client.connected()) {
   	    reconnect();
   	  }
   	  client.loop();

   	  persWM.handleWiFi();

   	  dnsServer.processNextRequest();
   	  server.handleClient();

   	  addPtToHist();

   	  delay(200);


}
