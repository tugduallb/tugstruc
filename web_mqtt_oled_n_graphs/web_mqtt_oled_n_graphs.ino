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
String temp_out = "--";
String temp_in = "--";
String pressure = "--";
String light_status = "0";
String mqttvalue;
WiFiUDP ntpUDP;
int contrast = 125;
int touch_count = 0;
int touch_sensibilite = 10;
int menu = 0;
const char* mqtt_server = "tugdutrucmqttserver";
const char* mqtt_username = "";
const char* mqtt_password = "";
const char* mqtt_topic = "sensors/test//erature";
// touch sensor
CapacitiveSensor   cs_4_5 = CapacitiveSensor(14,13);        // 10 megohm resistor between pins 4 & 6, pin 6 is sensor pin, add wire, foil

// By default 'time.nist.gov' is used with 60 seconds update interval and
// no offset
//NTPClient timeClient(ntpUDP);

// You can specify the time server pool and the offset, (in seconds)
// additionaly you can specify the update interval (in milliseconds).
//NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);
NTPClient timeClient(ntpUDP, "0.fr.pool.ntp.org", 7200, 60000);
//NTPClient timeClient2(ntpUDP, "0.fr.pool.ntp.org", 3600, 60000);
unsigned int mqttPort = 1883;       // the standard MQTT broker port
unsigned int max_subscriptions = 30;
unsigned int max_retained_topics = 30;

//U8G2_ST7920_128X64_1_SW_SPI u8g2(U8G2_R0, 13, 11, 10, 8);
U8G2_SSD1306_64X48_ER_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);


long intervalHist = 900000;
int     sizeHist = 100 ;        // Taille historique (7h x 12pts) - History size = ~3j
unsigned long previousMillis = intervalHist;

long intervalDisplayRefersh = 30000;
unsigned long previousMillisDisp = intervalDisplayRefersh;

StaticJsonBuffer<10000> jsonBuffer;      // Buffer static contenant le JSON courant - Current JSON static buffer
JsonObject& root = jsonBuffer.createObject();
JsonArray& timestamp = root.createNestedArray("timestamp");
JsonArray& hist_t_in = root.createNestedArray("temp_in");
JsonArray& hist_t_out = root.createNestedArray("temp_out");
JsonArray& hist_pa = root.createNestedArray("pressure");

StaticJsonBuffer<1000> jsonBufferConf;      // Buffer static contenant le JSON courant - Current JSON static buffer
JsonObject& conf = jsonBufferConf.createObject();
// JsonArray& param = conf.createNestedArray("param");
// JsonArray& value = conf.createNestedArray("value");

char json[10000];




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
  mqttvalue = "";
  int one_decimal = 0;
  for (int i=0;i<length;i++){
	 mqttvalue += String(data_str[i]);
  }
  Serial.print("mqttvalue: ");
  Serial.println(mqttvalue);
  //mqttvalue = String(data_str[0])+String(data_str[1])+String(data_str[2])+String(data_str[3]);
  if (strcmp( topic, "sonde_ext/temp")== 0) {
     temp_out = mqttvalue;
   }
   if (strcmp( topic, "sonde_int/temp")== 0) {
     temp_in = mqttvalue;
   }
   if (strcmp( topic, "sonde_int/pressure")== 0) {
     pressure = mqttvalue;
   }
   if (strcmp( topic, "switch/gpio/5")== 0) {
        light_status = mqttvalue;
      }
   if (strcmp( topic, "touch")== 0) {
	   touch_count = mqttvalue.toFloat();
      }

   if (strcmp( topic, "touch_s")== 0) {
	   touch_sensibilite = mqttvalue.toFloat();
         }

   if (strcmp( topic, "histo")== 0) {
	   sizeHist = mqttvalue.toFloat();
         }

   if (strcmp( topic, "refresh")== 0) {
	   intervalHist = mqttvalue.toFloat();
         }




  display_menu(menu);


}
WiFiClient espClient;
PubSubClient client(mqtt_server,1883,data_callback,espClient);

void addPtToHist(){
  unsigned long currentMillis = millis();

  if ( currentMillis - previousMillis > intervalHist ) {
    long int tps = timeClient.getEpochTime();
    previousMillis = currentMillis;
    if ((temp_out != "--") ||
    	(temp_in != "--") ||
		(pressure != "--")) {
      timestamp.add(tps);
      hist_t_in.add(double_with_n_digits(temp_in.toFloat(), 1));
      hist_t_out.add(double_with_n_digits(temp_out.toFloat(), 1));
      hist_pa.add(double_with_n_digits(pressure.toFloat()/100, 2));

      if ( hist_t_in.size() > sizeHist ) {
        timestamp.removeAt(0);
        hist_t_in.removeAt(0);
        hist_t_out.removeAt(0);
        hist_pa.removeAt(0);
      }
      delay(100);
      root.printTo(json, sizeof(json));
      root.prettyPrintTo(Serial);
      saveHistory();
    }
  }
}
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
	u8g2.drawStr(2, 10, "t in");
	u8g2.setFont(u8g2_font_helvB24_tn );
	u8g2.drawStr(2, 40, temp_in.c_str());
	u8g2.sendBuffer();
}
void oled_display_out() {
	u8g2.clearBuffer();
	u8g2.setFont(u8g2_font_smart_patrol_nbp_tr);
	u8g2.drawStr(2, 10, "t out");
	u8g2.setFont(u8g2_font_helvB24_tn );
	u8g2.drawStr(2, 40, temp_out.c_str());
	u8g2.sendBuffer();
}
void oled_display_pressure() {
	u8g2.clearBuffer();
	u8g2.setFont(u8g2_font_smart_patrol_nbp_tr);
	u8g2.drawStr(2, 10, "barro");
	u8g2.setFont(u8g2_font_helvB18_tn );
	u8g2.drawStr(2, 40, pressure.c_str());
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
        Serial.print("wifi connected - @IP=");
        Serial.println(WiFi.localIP());
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
        //addPtToHist();
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
	case 3:
		oled_display_pressure();
		break;
	default:
		oled_display();
	}
}
void loop(void) {
	Serial.print(".");

      long touch =  cs_4_5.capacitiveSensor(touch_sensibilite);

      if (touch >=0 ){
    	  touch_count ++;
    	  Serial.print("touch: ");
    	  Serial.println(touch_count);

      }
      else
      {
    	  //Serial.print("Touch:");Serial.print(touch);Serial.print(" seuil:");Serial.println(touch_sensibilite);


    	  if (touch_count >=6 ){
    		  if (light_status == "0") {
    			   client.publish("switch/gpio/5", "1");
    		   }
    		   else {
    			   client.publish("switch/gpio/5", "0");
    		   }
    	  }
    	  else if (touch_count >=3 ){
    	      	  contrast = contrast+125;
    	      	  if (contrast > 250) contrast = 0;
    	      	  Serial.print("contrast : ");
				  Serial.println(contrast);
    	      	  u8g2.setContrast(contrast);
    	  }
    	  else if (touch_count >=1){
				Serial.print("menu : ");
				Serial.println(menu);
			    menu = (menu+1)%4;
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
