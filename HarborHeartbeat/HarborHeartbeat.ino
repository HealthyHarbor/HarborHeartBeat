#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>



// HH Prototype which receives data from the Village Blue API and scrapes
// the lower dissolved oxygen reading
// Adding Heartbeat prototype component


int wifiStatus;
 
// WiFi information

const char WIFI_SSID[] = "ssid";
const char WIFI_PSK[] = "password";

 
// Remote site information
const char http_site[] = "18.216.116.112";
const int http_port = 5000;
 
// Global variables
WiFiClient client;

// I/O Pin Mapping
const byte d0 = 16; // d0 also writes to the onboard LED. PWM doesn't work on on-board LED, but does work for external LED
const byte d1 = 5;
const byte d2 = 4;
const byte d3 = 0; 
const byte d4 = 2; // also TXD1 - gets commands when uploading from from serial
const byte d5 = 14;
const byte d6 = 12;
const byte d7 = 13;
const byte d8 = 15;
const byte rx = 3; // also tied to serial from micro-USB during upload, does still work as an output
const byte tx = 1; // same as above

// pin configuration
byte pinR = d1;
byte pinG = d2;
byte pinB = d3;
byte maxPin = d5;
byte livePin = d7;
byte lowPin = d6;

// define beat RGB data structure
struct bRGB {
  byte r;
  byte g;
  byte b;
  byte bpm;
};


bool debug = 1; 
void setup() {
  
  // Set up serial console to read web page
if (debug) {
  Serial.begin(115200);
  Serial.print("Connecting to WiFi");
}  

  // Connect to WiFi
  connectWiFi();
      wifiStatus = WiFi.status();
      if(wifiStatus == WL_CONNECTED){
         Serial.println("");
         Serial.println("Connected.");  
         Serial.println("Your IP address is: ");
         Serial.println(WiFi.localIP());  
      }

  // Set up I/O pins  
  
  pinMode(maxPin, INPUT_PULLUP); 
  pinMode(livePin, INPUT_PULLUP); 
  pinMode(lowPin, INPUT_PULLUP);  
  pinMode(pinR, OUTPUT);
  pinMode(pinG, OUTPUT);
  pinMode(pinB, OUTPUT);
  
  writeRGB(0,0,0);
}

// intialize error code values;
int error = 0;
int warning = 0;
float reading;

int pollFreq = 20;
int count = pollFreq; 
//bool live;
bool lastState = 0;

void loop() {
  bRGB Beat;
  byte N;
  bool live = !digitalRead(livePin);
  bool maxMode = !digitalRead(maxPin);
  bool low = !digitalRead(lowPin);

  if ( error != 1 ) {
    if(live){
      Serial.println("Live data mode");
       // connect to web data every 10 iterations
      if (count == pollFreq) {
      // Attempt to connect to website
        if ( !getPage() ) {
          if (debug) {
            Serial.println("GET request failed");
          }
          if( !WiFi.status() ) {
              // error code 1: WiFi disconnected
              error = 1;       
          }
  
          else {
            if ( !getPage() ) {
              if (debug) {
                Serial.println("GET request failed");
              }
            warning = 1;
            }           
            else{
            warning = 0;
            } 
          }
     
        }
        }
        // If the page was successful, 
      if ( error == 0 && warning == 0 && count == pollFreq ) {
        reading = readAPI();
        if (debug) {
          Serial.print("Dissolved Oxygen is: ");
          Serial.println(reading);
          Serial.println();
        }
        count = 0;
        }
        
      
      Beat = beatVals(reading); 
      count++;
      }

      // Sets example of reading below threashold
      else if (low) {
        
      if (debug) {
        Serial.println("Low mode");
      }
      
      Beat = beatVals(4.9); 
      count = pollFreq;
    }
      // sets example maximum at 15 DO
      else if (maxMode) {
        if (debug) {
          Serial.println("Max mode");
        }
      Beat = beatVals(15); 
      count = pollFreq;
    }  
  
    pulse(Beat);

  }
  else {
    connectWiFi();
        wifiStatus = WiFi.status();
        if(wifiStatus == WL_CONNECTED){
          if (debug) {
           Serial.println("");
           Serial.println("Connected.");  
           Serial.println("Your IP address is: ");
           Serial.println(WiFi.localIP()); 
          } 
        }    
  }
  
  
  //writeRGB(0, 0, 0); // hold lowest value

  //delay(100);
  
}

float readAPI(){

  char inData[50];
  char c;

  // writes to inData with extra characters; value needed is the only numeric info
  byte lastIndex = getData(inData);

    // find floating point number
    int lNum = 0;
    char charVal[5];
    for(int i = 0; i <= lastIndex; i++){

      c = inData[i];
      if (isDigit(c) or c == '.' ){
        charVal[lNum] = inData[i];
        lNum++;
      }
      
    }
    charVal[lNum] = '\0';

    float value = atof(charVal);
    
  
//    Serial.println();
//    Serial.println("Char value:");
//    Serial.println(charVal);
//    Serial.print("Value: ");
//    Serial.println(value);

    return value;
    
  }


// WiFi Functions
  // Read all available client information:
int getData(char *dataRef){ 
  
  char state = '\0';
  byte lastIndex;
  while ( client.connected() ){

    byte index = 0;
    bool test = false;
    // If there are incoming bytes, print them
    while ( client.available() ) {
    
     char c = client.read();

  
  
      // Test what state the scraper is in. possible states:
      // '/0' = not a candidate
      // 'c' = c found, check for e
      // 'e' = ce found, check for n
      // 'n' = cen found. concatenate string search q
      // 'q' =  q found, stop string. finish reading the rest of the response
  
      if(state == '\0') {
        test = c == 'c';
        if(test){
          state = 'c';
        }
      
      }
      else if(state == 'c'){
        test = c == 'e';
        if(test){
          state = 'e';
        }
        else{
        state = '\0';     
        }
      }
      else if(state == 'e'){
        test = c == 'n';
        if(test){
          state = 'n';
          dataRef[0] = c;
          index++;
        }
        else{
        state = '\0';     
        }      
      }
      else if(state == 'n'){
        test = c == 'q';
        if(test){
          state = 'q';
          dataRef[index] = '\0';
          lastIndex = index - 1;
        }
        else{
         dataRef[index] = c;
         index++; 
        }         
      }
      else {    
  
      }
    }
  }
  

  // If the server has disconnected, stop the client
  // and return length of string
  if ( !client.connected() ) {

    // Close socket
    client.stop();
    return lastIndex;
  }
    
} 
 
// Attempt to connect to WiFi
void connectWiFi() {


  
 // byte led_status = 0;
  
  // Set WiFi mode to station (client)
  WiFi.mode(WIFI_STA);
  
  // Initiate connection with SSID and PSK
  WiFi.begin(WIFI_SSID, WIFI_PSK);
  
  // Print "." while we wait for WiFi connection
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        if (debug){
        Serial.print(".");
        }
      } 
}
 
// Perform an HTTP GET request to a remote page
bool getPage() {
  
  // Attempt to make a connection to the remote server
  if ( !client.connect(http_site, http_port) ) {
    return false;
  }
  
  // Make an HTTP GET request
  client.println("GET /harbor HTTP/1.1");
  client.print("Host: ");
  client.println(http_site);
  client.println("Connection: close");
  client.println();
  
  return true;
}

// I/O Functions:
bRGB beatVals(float DO){
  bRGB Beat;
  Beat.bpm = round(200 - (10*DO));// min 50;

  if(DO <= 5){
    Beat.r = round((255/10)*(10-DO));
    Beat.g = round((255/10)*(DO));
    Beat.b = 0;
  }
  else if (DO <=15){
    int s = DO - 5;
    Beat.r = 0;
    Beat.g = round((255/10)*(10-s));
    Beat.b = round((255/10)*(s));
  }
  else {
    Beat.r = 0;
    Beat.g = 0;
    Beat.b = 255;
  }

  return Beat;
} 


// generates timing and brightening,writes to RGB outputs
int pulse(bRGB Beat){

  // values for RGB and beats per minute
  int R = Beat.r;
  int G = Beat.g;
  int B = Beat.b;
  int bpm = Beat.bpm;
  
  int n = 10; // decides how many colors are in the transition
  
  int Rs[n];// initialize vectors for each color
  int Gs[n];
  int Bs[n];
  
  int incR = round(R/n); // set the amount to increment
  int incG = round(G/n);
  int incB = round(B/n); 
  
  Rs[0] = {incR};
  Gs[0] = {incG};
  Bs[0] = {incB};
  
  for(int i=1; i<=n; i++){ // fill vector with increasing blue intensity
    Bs[i] = {Bs[i-1]+incB};
   }
  
  for(int i=1; i<=n; i++){
    Gs[i] = {Gs[i-1]+incG};
    }
  
  for(int i=1; i<=n; i++){
    Rs[i] = {Rs[i-1]+incR};
    }
    
  int tStage = round(60000/(bpm*4)); // set timing for each "stage" of the pulse
  int t = round(tStage/n); // set timing for incremental sections
  
  for(int i=0; i<n; i++){ // brighten
    writeRGB((Rs[i]), (Gs[i]), (Bs[i])); // function writeRGB declared below
    delay(t);
    }
  
  writeRGB(R, G, B); // hold brightest value
  delay(tStage*3);
  
  for(int i=n; i>=0; i--){ // dim
    writeRGB((Rs[i]), (Gs[i]), (Bs[i]));
    delay(t);
   }

  writeRGB(0, 0, 0); // hold lowest value
  delay(tStage);

}

int writeRGB(int r, int g, int b){ // function for writing to RGB LED
int r2 = (r+1)*4-1;
int g2 = (g+1)*4-1;
int b2 = (b+1)*4-1;

analogWrite(d1, r2);
analogWrite(d2, g2);
analogWrite(d3, b2);
}
