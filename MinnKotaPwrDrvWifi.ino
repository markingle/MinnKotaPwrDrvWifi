#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <WebSocketsServer.h>
#include <WebSocketClient.h>


// state machine states
unsigned int state;
#define SEQUENCE_IDLE 0x00
#define GET_SAMPLE 0x10

#define GET_SAMPLE__WAITING 0x12

uint8_t remote_ip;
uint8_t socketNumber;
float value;

const char WiFiAPPSK[] = "Mk";

#define USE_SERIAL Serial
#define DBG_OUTPUT_PORT Serial

//These will need to be updated to the GPIO pins for each control circuit.
int POWER = 5; //PIN D1
int MOMENTARY = 4; //PIN D2
int SPEED = 14; // PIN D5
int LEFT = 12; // PIN D6
int RIGHT = 13; // PIN D7
const int ANALOG_PIN = A0;

//boolean LED1State = false;
//boolean LED2State = false;

//long buttonTimer = 0;
//long longPressTime = 250;

//boolean buttonActive = false;
//boolean longPressActive = false;

ESP8266WebServer server(80);

// Create a Websocket server
WebSocketsServer webSocket(81);

extern "C" {
uint16 readvdd33(void);
}

void handleRoot() {
  server.send(200, "text/html", "<h1>You are connected</h1>");
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {
    String text = String((char *) &payload[0]);
    char * textC = (char *) &payload[0];
    String voltage;
    String temp;
    int nr;
    int on;
    uint32_t rmask;
    int i;
    
    switch(type) {
        case WStype_DISCONNECTED:
            //Reset the control for sending samples of ADC to idle to allow for web server to respond.
            USE_SERIAL.printf("[%u] Disconnected!\n", num);
            state = SEQUENCE_IDLE;
            break;
        case WStype_CONNECTED:
          {
            //Display client IP info that is connected in Serial monitor and set control to enable samples to be sent every two seconds (see analogsample() function)
            IPAddress ip = webSocket.remoteIP(num);
            USE_SERIAL.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
            socketNumber = num;
            state = GET_SAMPLE;
          }
            break;
 
        case WStype_TEXT:
            if (payload[0] == '#')
              {
                value=readvdd33();
                Serial.print("Vcc:");
                Serial.println(value/1000);
                voltage = String(value/1000);
                webSocket.sendTXT(num, voltage);
                Serial.printf("[%u] Digital GPIO Control Msg: %s\n", num, payload);
                if (payload[1] == 'I')
                {
                  if (payload[2] == 'D')
                    {
                    Serial.printf("Direction Down");
                    digitalWrite(POWER, HIGH);
                    }
                  if (payload[2] == 'U')
                    {
                    Serial.printf("Direction Up");
                    digitalWrite(POWER, LOW);
                    }
                  break;
                }
                if (payload[1] == 'M')
                {
                  if (payload[2] == 'D')
                    {
                    Serial.printf("Direction Down");
                    digitalWrite(MOMENTARY, HIGH);
                    }
                  if (payload[2] == 'U')
                    {
                    Serial.printf("Direction Up");
                    digitalWrite(MOMENTARY, LOW);
                    }
                  break;
                }
                if (payload[1] == 'L')
                {
                  if (payload[2] == 'D')
                    {
                    Serial.printf("Direction Down");
                    digitalWrite(LEFT, HIGH);
                    }
                  if (payload[2] == 'U')
                    {
                    Serial.printf("Direction Up");
                    digitalWrite(LEFT, LOW);
                    }
                  break;
                 }
                 if (payload[1] == 'R')
                 {
                  if (payload[2] == 'D')
                    {
                    Serial.printf("Direction Down");
                    digitalWrite(RIGHT, HIGH);
                    }
                  if (payload[2] == 'U')
                    {
                    Serial.printf("Direction Up");
                    digitalWrite(RIGHT, LOW);
                    }
                  break;
                 }
              }
            if (payload[0] == 'S')
              {
                Serial.printf("[%u] Analog GPIO Control Msg: %s\n", num, payload);
              }
              
         case WStype_BIN:
         {
            USE_SERIAL.printf("[%u] get binary lenght: %u\n", num, lenght);
            //hexdump(payload, lenght);
            //analogWrite(13,atoi((const char *)payload));
            Serial.printf("Payload");
            Serial.printf("[%u] Analog GPIO Control Msg: %s\n", num, payload);
            Serial.println((char *)payload);
            int temp = atoi((char *)payload);
            uint32_t rgb = (uint32_t) strtol((const char *) &payload[1], NULL, 16);
            analogWrite(SPEED,temp);
            webSocket.sendTXT(num,"Got Speed Change");
            Serial.printf("Intger %u\n", temp);
         }
         break;
         
         case WStype_ERROR:
            USE_SERIAL.printf(WStype_ERROR + " Error [%u] , %s\n",num, payload); 
    }
}

String getContentType(String filename){
  if(server.hasArg("download")) return "application/octet-stream";
  else if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  else if(filename.endsWith(".svg")) return "image/svg+xml";
  return "text/plain";
}

bool handleFileRead(String path){
  DBG_OUTPUT_PORT.println("handleFileRead: " + path);
  if(path.endsWith("/"))
    {
      path += "relay.html";
      state = SEQUENCE_IDLE;
    }
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  DBG_OUTPUT_PORT.println("PathFile: " + pathWithGz);
  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)){
    if(SPIFFS.exists(pathWithGz))
      path += ".gz";
    File file = SPIFFS.open(path, "r");
    size_t sent = server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void setupWiFi()
{
  WiFi.mode(WIFI_AP);
  
  String AP_NameString = "MinnKotaPowerDrive";

  char AP_NameChar[AP_NameString.length() + 1];
  memset(AP_NameChar, 0, AP_NameString.length() + 1);

  for (int i=0; i<AP_NameString.length(); i++)
    AP_NameChar[i] = AP_NameString.charAt(i);

  WiFi.softAP(AP_NameChar, WiFiAPPSK);
}

void setup() {

  pinMode(POWER, OUTPUT);
  pinMode(MOMENTARY, OUTPUT);
  pinMode(SPEED, OUTPUT);
  pinMode(LEFT, OUTPUT);
  pinMode(RIGHT, OUTPUT);

  digitalWrite(POWER, LOW);
  digitalWrite(MOMENTARY, LOW);
  digitalWrite(SPEED, LOW);
  digitalWrite(LEFT, LOW);
  digitalWrite(RIGHT, LOW);
  
  Serial.begin(115200);
  SPIFFS.begin();
  Serial.println();
  Serial.print("Configuring access point...");
  /* You can remove the password parameter if you want the AP to be open. */
  setupWiFi();
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  server.on("/", HTTP_GET, [](){
    handleFileRead("/");
  });

//Handle when user requests a file that does not exist
  server.onNotFound([](){
    if(!handleFileRead(server.uri()))
  server.send(404, "text/plain", "FileNotFound");
  });

 
  // start webSocket server
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  
  server.begin();
  Serial.println("HTTP server started");
  
 

  int voltage = value/1000;

//+++++++ MDNS will not work when WiFi is in AP mode but I am leave this code in place incase this changes++++++
//if (!MDNS.begin("esp8266")) {
//    Serial.println("Error setting up MDNS responder!");
//    while(1) { 
//      delay(1000);
//    }
//  }
//  Serial.println("mDNS responder started");

  // Add service to MDNS
  //  MDNS.addService("http", "tcp", 80);
  //  MDNS.addService("ws", "tcp", 81);
}

void loop() {
  webSocket.loop();
  server.handleClient();
}
