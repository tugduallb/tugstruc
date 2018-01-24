#include <Arduino.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "uMQTTBroker.h"
#include <CapacitiveSensor.h>

#include <PersWiFiManager.h>
#include <ArduinoJson.h>
#include <EasySSDP.h>
#include <SPIFFSReadServer.h>
#include <DNSServer.h>
#include <FS.h>
#define DEVICE_NAME "tugdutruc"
#define HISTORY_FILE "/history.json"
#define CONFIG_FILE "/config.json"

//server objects
SPIFFSReadServer server(80);
DNSServer dnsServer;
PersWiFiManager persWM(server, dnsServer);
StaticJsonBuffer<10000> jsonBuffer;      // Buffer static contenant le JSON courant - Current JSON static buffer
JsonObject& root = jsonBuffer.createObject();
JsonArray& timestamp = root.createNestedArray("timestamp");
JsonArray& hist_t = root.createNestedArray("temp");

StaticJsonBuffer<1000> jsonBufferConf;      // Buffer static contenant le JSON courant - Current JSON static buffer
JsonObject& conf = jsonBufferConf.createObject();
// JsonArray& param = conf.createNestedArray("param");
// JsonArray& value = conf.createNestedArray("value");

char json[10000];
char jsonConf[1000];                   // Buffer pour export du JSON - JSON export buffer
long intervalHist = 30000;  // 12 mesures / heure
int     sizeHist = 840 ;        // Taille historique (7h x 12pts) - History size = ~3j
unsigned long previousMillis = intervalHist;  // Dernier point enregistré dans l'historique
unsigned int offsetThermometer = 0; // offset to apply to temperature
int signe_offset = 0; // 0 = positif

CapacitiveSensor   cs_4_5 = CapacitiveSensor(14,13);        // 10 megohm resistor between pins 4 & 6, pin 6 is sensor pin, add wire, foil


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


void data_callback(uint32_t *client /* we can ignore this */, const char* topic, uint32_t topic_len, const char *data, uint32_t lengh) {
  char topic_str[topic_len + 1];
  os_memcpy(topic_str, topic, topic_len);
  topic_str[topic_len] = '\0';

  char data_str[lengh + 1];
  os_memcpy(data_str, data, lengh);
  data_str[lengh] = '\0';

  Serial.print("received topic '");
  Serial.print(topic_str);
  Serial.print("' with data '");
  Serial.print(data_str);
  Serial.println("'");
  temp = String(data_str[0])+String(data_str[1])+String(data_str[2])+String(data_str[3]);
  if (strcmp( topic, "sensor/temp_in")== 0) {
    temp_out = temp;
  }
  if (strcmp( topic, "sensor/temp")== 0) {
    temp_in = temp;
  }
  display_menu(menu);
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

void setup(void) {
  u8g2.begin();
  u8g2.setContrast(contrast);
  Serial.begin(115200);
  oled_display_start("Tugdu", 0, 10);
  oled_display_start("r", 0, 20);
  oled_display_start("u", 0, 30);
  oled_display_start("c", 0, 40);

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
        loadHistory();
      }
      delay(50);
      loadConfig();
      loadHistory();
      //sets network name for AP mode
      persWM.setApCredentials(DEVICE_NAME);
      //persWM.setApCredentials(DEVICE_NAME, "password"); optional password

      //make connecting/disconnecting non-blocking
      persWM.setConnectNonBlock(true);

      //in non-blocking mode, program will continue past this point without waiting
      persWM.begin();


      //handles commands from webpage, sends live data in JSON format
      server.on("/graph_temp.json", sendHistory);
      server.on("/config.json", configure);


//  // We start by connecting to a WiFi network
//  Serial.print("Connecting to ");
//  Serial.println(ssid);
//  WiFi.begin(ssid, pass);
//
//  while (WiFi.status() != WL_CONNECTED) {
//	delay(500);
//    Serial.print(".");
//  }
//  Serial.println("");
//
//  Serial.println("WiFi connected");
//  Serial.println("IP address: ");
//  Serial.println(WiFi.localIP());

  timeClient.begin();
  /*
     Register the callback
  */
  MQTT_server_onData(data_callback);

  /*
     Start the broker
  */
  Serial.println("Starting MQTT broker");
  MQTT_server_start(mqttPort, max_subscriptions, max_retained_topics);

  /*
     Subscribe to anything
  */
  MQTT_local_subscribe((unsigned char *)"#", 0);



   /* server.on("/wifi", []() {
      Serial.print("server.on /wifi");
      if (server.hasArg("x")) {
        x = server.arg("x").toInt();
        Serial.print(String("x: ") + x);
      } //if
      if (server.hasArg("y")) {
        y = server.arg("y");
        Serial.print("y: " + y);
      } //if

      //build json object of program data
      StaticJsonBuffer<200> jsonBuffer;
      JsonObject &jsonAP = jsonBuffer.createObject();
      t = getTemperature();
      jsonAP["x"] = x;
      jsonAP["y"] = y;

      char jsonchar[200];
      jsonAP.printTo(jsonchar); //print to char array, takes more memory but sends in one piece

      server.send(200, "application/json", jsonchar);

      }); //server.on wifi*/

//      server.on("/temp", []() {
//      Serial.print("server.on /temp");
//
//      //build json object of program data
//      StaticJsonBuffer<200> jsonBuffer;
//      JsonObject &jsonAP = jsonBuffer.createObject();
//      t = getTemperature();
//
//      String json_now = "{\"temp\":\"" + String(getTemperature()) + "\",";
//      json_now += "\"timestamp\":\"" + String(NTP.getTime()) + "\"}";
//      server.send(200, "application/json", json_now);
//      addPtToHist();
//      //sendHistory();
//      //Serial.print("send json");
//      Serial.print(json_now);
//
//
//
//    }); //server.on temp


    server.begin();
    Serial.print("setup complete.");
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
void configure(){
  Serial.print("configure");
  Serial.print(server.args());
  String message = "";

  for (int i = 0; i < server.args(); i++) {

    message += server.argName(i) + ":" + server.arg(i) + "\n";
    JsonArray& param = conf.createNestedArray(server.argName(i));
    param.add(server.arg(i));
    }
    conf.prettyPrintTo(Serial);

  String jsonConf = "{ \"OK\"}";
  server.send(200, "application/json", jsonConf);
  Serial.print("------- end configure");

  saveConfig();
  loadConfig();
}
void sendConfig(){
  Serial.print("Send Config");
  Serial.print(jsonConf);
  conf.printTo(jsonConf, sizeof(jsonConf));             // Export du JSON dans une chaine - Export JSON object as a string
  server.send(200, "application/json", jsonConf);   // Envoi l'historique au client Web - Send history data to the web client
  Serial.print("------- fin sendConfig");
}
String searchString(char * str){
    String found_str = String();
    int startString = 0;
     for (int i=0;i<strlen(str);i++){
        if (startString == 1 && str[i] != '"'){
            found_str = found_str + str[i];
        }

       if (startString == 1 && str[i] == '"'){
           startString = 0;
        }
        else{
            if (str[i] != '['){
            startString = 1;
            }
        }
     }
     return found_str;

}
void loadConfig(){
  char p[100];
  char v[100];
  File file = SPIFFS.open(CONFIG_FILE, "r");
  if (!file){
    Serial.println("Aucun config existe");
  } else {
    size_t size = file.size();
    if ( size == 0 ) {
      Serial.println("Fichier config vide!");
    } else {
      std::unique_ptr<char[]> buf (new char[size]);
      file.readBytes(buf.get(), size);
      JsonObject& conf = jsonBuffer.parseObject(buf.get());
      if (!conf.success()) {
        Serial.println("Impossible de lire le JSON - Impossible to read JSON file");
      } else {
        Serial.println("config charge ");
        conf.prettyPrintTo(Serial);

        if (conf.containsKey("refresh"))
        {
            String refresh = conf["refresh"];
            refresh.replace("[\"", "");
            refresh.replace("\"]", "");
            //conf["refresh"].printTo(value, sizeof(value));
            intervalHist = refresh.toInt() * 1000 * 60;
            Serial.print("refresh");
            Serial.print(refresh);
            Serial.print(intervalHist);
        }

        if (conf.containsKey("offset"))
        {
            offsetThermometer = 0;

            String offset = conf["offset"];
            offset.replace("[\"", "");
            offset.replace("\"]", "");

            if (offset.indexOf("-") >=0){
                Serial.print("négatif");
                signe_offset = 1;
                offset.replace("-", "");
            }
            else{
                signe_offset = 0;
            }

            offsetThermometer = offset.toInt();


            Serial.print("offset");
            Serial.print(offset);
            Serial.print(offsetThermometer);
        }
      }
    }
    file.close();
  }
}
void saveConfig(){
  Serial.print("Save Config");
  bool rm = SPIFFS.remove(CONFIG_FILE);
  File configFile = SPIFFS.open(CONFIG_FILE, "w");

  conf.printTo(configFile); // Exporte et enregsitre le JSON dans la zone SPIFFS - Export and save JSON object to SPIFFS area
  configFile.close();
  //Serial.print(conf);
  Serial.print("------- fin saveConfig");
}
void sendHistory(){
  //root.printTo(json, sizeof(json));
  server.send(200, "application/json", json);   // Envoi l'historique au client Web - Send history data to the web client
  //Serial.print("Send History");
  //Serial.print(json);
  //root.prettyPrintTo(Serial);
  //Serial.print("------- fin sendHistory");
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
        Serial.println("Historique charge - History loaded");
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
  Serial.print("----------------");
  Serial.print("| Save History |");
  Serial.print("----------------");
  File historyFile = SPIFFS.open(HISTORY_FILE, "w");
  root.printTo(historyFile); // Exporte et enregsitre le JSON dans la zone SPIFFS - Export and save JSON object to SPIFFS area
  historyFile.close();
  // root.prettyPrintTo(Serial);
  Serial.print("------- fin saveHistory");
}

void calcStat(){
  float statTemp[7] = {-999,-999,-999,-999,-999,-999,-999};
  int nbClass = 7;  // Nombre de classes - Number of classes
  int currentClass = 0;
  int sizeClass = hist_t.size() / nbClass;  // 2
  double temp;
  //
  if ( hist_t.size() >= sizeHist ) {
	//Serial.print("taille classe ");Serial.println(sizeClass);
	//Serial.print("taille historique ");Serial.println(hist_t.size());
	for ( int k = 0 ; k < hist_t.size() ; k++ ) {
	  temp = root["temp"][k];
	  if ( statTemp[currentClass] == -999 ) {
		statTemp[ currentClass ] = temp;
	  } else {
		statTemp[ currentClass ] = ( statTemp[ currentClass ] + temp ) / 2;
	  }

	  if ( ( k + 1 ) > sizeClass * ( currentClass + 1 ) ) {
		//Serial.print("k ");Serial.print(k + 1);Serial.print(" Cellule statTemp = ");Serial.println(statTemp[ currentClass ]);
		currentClass++;
	  } else {
		//Serial.print("k ");Serial.print(k + 1);Serial.print(" < ");Serial.println(sizeClass * currentClass);
	  }
	}

  }
}
void addPtToHist(){
  unsigned long currentMillis = millis();
  //Serial.print("addPtToHist");
  //root.prettyPrintTo(Serial);
  //Serial.print(intervalHist);

  if ( currentMillis - previousMillis > intervalHist ) {
	  //Serial.print(intervalHist);
	long int tps = timeClient.getEpochTime();
	//Serial.print(tps);
	previousMillis = currentMillis;
   // Serial.print(NTP.getTime());
	if ( tps > 0 ) {
	  timestamp.add(tps);
	  hist_t.add(double_with_n_digits(temp_in.toFloat(), 1));

	  if ( hist_t.size() > sizeHist ) {
	   // Serial.print("efface anciennes mesures");
		timestamp.removeAt(0);
		hist_t.removeAt(0);
	  }
	 // Serial.print("size hist_t ");Serial.print(hist_t.size());
	 //Serial.print("addPtToHist");
	  //calcStat();
	  delay(100);
	  root.printTo(json, sizeof(json));
	  saveHistory();
	  Serial.print(json);
	}
  }
  //Serial.print("fin");
  //root.prettyPrintTo(Serial);
  //Serial.print("---------- fin addPtToHist");
}


void loop(void) {
  //oled_display("T",25,25);
  

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
        //Serial.print("addPtToHist");
        //root.prettyPrintTo(Serial);
        //Serial.print(intervalHist);

        if ( currentMillis - previousMillis > intervalHist ) {
        	previousMillis = currentMillis;

        	display_menu(menu);
        }

      //delay(200);

      persWM.handleWiFi();

        dnsServer.processNextRequest();
        server.handleClient();

        addPtToHist();
        delay(200);

}
