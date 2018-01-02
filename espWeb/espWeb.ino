/*
SPIFFS-served REST API example for PersWiFiManager v3.0
*/

#define DEBUG_SERIAL //uncomment for Serial debugging statements

#ifdef DEBUG_SERIAL
#define DEBUG_BEGIN Serial.begin(115200)
#define DEBUG_PRINT(x) Serial.println(x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_BEGIN
#endif

//includes
#include <PersWiFiManager.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>

#include <EasySSDP.h> // http://ryandowning.net/EasySSDP/

//extension of ESP8266WebServer with SPIFFS handlers built in
#include <SPIFFSReadServer.h> // http://ryandowning.net/SPIFFSReadServer/
// upload data folder to chip with Arduino ESP8266 filesystem uploader
// https://github.com/esp8266/arduino-esp8266fs-plugin

#include <DNSServer.h>
#include <FS.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <NtpClientLib.h>

#define DEVICE_NAME "papa"
#define ONE_WIRE_BUS D3 // D1
#define HISTORY_FILE "/history.json"
#define CONFIG_FILE "/config.json"

//server objects
SPIFFSReadServer server(80);
DNSServer dnsServer;
PersWiFiManager persWM(server, dnsServer);

////// Sample program data
int x;
String y;
float   t = 0 ;
int     sizeHist = 840 ;        // Taille historique (7h x 12pts) - History size = ~3j
// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature DS18B20(&oneWire);
char temperatureCString[6];
char temperatureFString[6];

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
long intervalHist = 1000;  // 12 mesures / heure
unsigned long previousMillis = intervalHist;  // Dernier point enregistré dans l'historique 
unsigned int offsetThermometer = 0; // offset to apply to temperature
int signe_offset = 0; // 0 = positif


void setup() {
  DEBUG_BEGIN; //for terminal debugging
  DEBUG_PRINT();
  
  // NTP configurationNTP
  NTP.onNTPSyncEvent([](NTPSyncEvent_t error) {
    if (error) {
      Serial.print("Time Sync error: ");
      if (error == noResponse)
        Serial.println("NTP server not reachable");
      else if (error == invalidAddress)
        Serial.println("Invalid NTP server address");
      }
    else {
      Serial.print("Got NTP time: ");
      Serial.println(NTP.getTimeDateString(NTP.getLastNTPSync()));
    }
  });
  // Serveur NTP, decalage horaire, heure été - NTP Server, time offset, daylight 
  NTP.begin("pool.ntp.org", 0, true); 
  NTP.setInterval(60000);
  delay(500);
  
  //optional code handlers to run everytime wifi is connected...
  persWM.onConnect([]() {
    DEBUG_PRINT("wifi connected");
    DEBUG_PRINT(WiFi.localIP());
    EasySSDP::begin(server);
  });
  //...or AP mode is started
  persWM.onAp([](){
    DEBUG_PRINT("AP MODE");
    DEBUG_PRINT(persWM.getApSsid());
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
  
  server.on("/wifi", []() {
    DEBUG_PRINT("server.on /wifi");
    if (server.hasArg("x")) {
      x = server.arg("x").toInt();
      DEBUG_PRINT(String("x: ") + x);
    } //if
    if (server.hasArg("y")) {
      y = server.arg("y");
      DEBUG_PRINT("y: " + y);
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
    
    }); //server.on wifi
    
    server.on("/temp", []() {
    DEBUG_PRINT("server.on /temp");

    //build json object of program data
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject &jsonAP = jsonBuffer.createObject();
    t = getTemperature();
      
    String json_now = "{\"temp\":\"" + String(getTemperature()) + "\",";
    json_now += "\"timestamp\":\"" + String(NTP.getTime()) + "\"}";
    server.send(200, "application/json", json_now);
    addPtToHist();
    //sendHistory();
    //DEBUG_PRINT("send json");
    DEBUG_PRINT(json_now);
    
    

  }); //server.on temp


  server.begin();
  DEBUG_PRINT("setup complete.");
} //void setup

void configure(){
  DEBUG_PRINT("configure");
  DEBUG_PRINT(server.args());
  String message = "";

  for (int i = 0; i < server.args(); i++) {

    message += server.argName(i) + ":" + server.arg(i) + "\n";
    JsonArray& param = conf.createNestedArray(server.argName(i));
    param.add(server.arg(i));
    } 
    conf.prettyPrintTo(Serial);  
  
  String jsonConf = "{ \"OK\"}";
  server.send(200, "application/json", jsonConf);
  DEBUG_PRINT("------- end configure");
  
  saveConfig();
  loadConfig();
}
void sendConfig(){  
  DEBUG_PRINT("Send Config");
  DEBUG_PRINT(jsonConf);
  conf.printTo(jsonConf, sizeof(jsonConf));             // Export du JSON dans une chaine - Export JSON object as a string
  server.send(200, "application/json", jsonConf);   // Envoi l'historique au client Web - Send history data to the web client
  DEBUG_PRINT("------- fin sendConfig");   
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
            DEBUG_PRINT("refresh");
            DEBUG_PRINT(refresh);     
            DEBUG_PRINT(intervalHist);                 
        }
        
        if (conf.containsKey("offset"))
        {
            offsetThermometer = 0;
            
            String offset = conf["offset"];
            offset.replace("[\"", "");
            offset.replace("\"]", "");
            
            if (offset.indexOf("-") >=0){
                DEBUG_PRINT("négatif");
                signe_offset = 1;
                offset.replace("-", "");
            }
            else{
                signe_offset = 0;
            }

            offsetThermometer = offset.toInt();

            
            DEBUG_PRINT("offset");
            DEBUG_PRINT(offset);   
            DEBUG_PRINT(offsetThermometer);            
        }        
      }
    }
    file.close();
  }
}
void saveConfig(){
  DEBUG_PRINT("Save Config");            
  bool rm = SPIFFS.remove(CONFIG_FILE);
  File configFile = SPIFFS.open(CONFIG_FILE, "w");
  
  conf.printTo(configFile); // Exporte et enregsitre le JSON dans la zone SPIFFS - Export and save JSON object to SPIFFS area
  configFile.close();  
  //DEBUG_PRINT(conf);   
  DEBUG_PRINT("------- fin saveConfig");   
}


void sendHistory(){  
  //root.printTo(json, sizeof(json));
  server.send(200, "application/json", json);   // Envoi l'historique au client Web - Send history data to the web client
  //DEBUG_PRINT("Send History");
  //DEBUG_PRINT(json);
  //root.prettyPrintTo(Serial);
  //DEBUG_PRINT("------- fin sendHistory");   
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
  DEBUG_PRINT("----------------");    
  DEBUG_PRINT("| Save History |");            
  DEBUG_PRINT("----------------");    
  File historyFile = SPIFFS.open(HISTORY_FILE, "w");
  root.printTo(historyFile); // Exporte et enregsitre le JSON dans la zone SPIFFS - Export and save JSON object to SPIFFS area
  historyFile.close();  
  // root.prettyPrintTo(Serial); 
  DEBUG_PRINT("------- fin saveHistory");   
}

float getTemperature() {
  //DEBUG_PRINT("Get Temperature");
  float tempC;
  float tempF;
  do {
    DS18B20.requestTemperatures(); 
    tempC = DS18B20.getTempCByIndex(0);
    dtostrf(tempC, 2, 2, temperatureCString);
    tempF = DS18B20.getTempFByIndex(0);
    dtostrf(tempF, 3, 2, temperatureFString);
    delay(100);
  } while (tempC == 85.0 || tempC == (-127.0));
  //Serial.println ( tempC );
  //DEBUG_PRINT(tempC);
  //DEBUG_PRINT(offsetThermometer);
  
  
  if (signe_offset == 1){
      tempC = tempC - offsetThermometer;
    }
    else{
      tempC = tempC + offsetThermometer;
    }
    //DEBUG_PRINT(tempC);
    //DEBUG_PRINT("------- fin getTemperature");
    
    return tempC;
      
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
  //DEBUG_PRINT("addPtToHist");
  //root.prettyPrintTo(Serial);
  //DEBUG_PRINT(intervalHist);
  
  if ( currentMillis - previousMillis > intervalHist ) {
      //DEBUG_PRINT(intervalHist);
    long int tps = NTP.getTime();
    //DEBUG_PRINT(tps);
    previousMillis = currentMillis;
   // DEBUG_PRINT(NTP.getTime());
    if ( tps > 0 ) {
      timestamp.add(tps);
      hist_t.add(double_with_n_digits(t, 1));

      if ( hist_t.size() > sizeHist ) {
       // DEBUG_PRINT("efface anciennes mesures");
        timestamp.removeAt(0);
        hist_t.removeAt(0);
      }
     // DEBUG_PRINT("size hist_t ");DEBUG_PRINT(hist_t.size());
     //DEBUG_PRINT("addPtToHist");
      //calcStat();
      delay(100);
      root.printTo(json, sizeof(json));
      saveHistory();
      DEBUG_PRINT(json);  
    }  
  }
  //DEBUG_PRINT("fin");
  //root.prettyPrintTo(Serial);
  //DEBUG_PRINT("---------- fin addPtToHist");
}

void loop() {
  //in non-blocking mode, handleWiFi must be called in the main loop
  persWM.handleWiFi();

  dnsServer.processNextRequest();
  server.handleClient();
  
  addPtToHist();
  delay(100);
  
  //DEBUG_PRINT(millis());

  // do stuff with x and y

} //void loop


