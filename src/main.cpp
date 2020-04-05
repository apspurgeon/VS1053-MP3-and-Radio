  
// D0 (16) - CS - X-CS on VS1053
// D1 (5) - LEDs (FastLED) via level shifter   
// D2 (4) - CardCS - CS on VS1053
// D3 (0) - DREQ
// D4 (2) - XRESET
// D5 (14) - SCK
// D6 (12) - MISO
// D7 (13) - MOSI
// D8 (15) - DCS - X-DCS on VS1053
// A0 (A0) - Input for track selection (rotary switch)

//Rotary switch.  Wait for 500ms on any A0 changes to allow for selection movement.
//Pos 0:  5v Power off
//Pos 1:  5v Power on  - A0 to 0v - Stop playing
//Pos 2:  Radio NZ - A0 Resister divider 1
//Pos 3:  BBC world service - A0 Resister divider 2
//Pos 4:  WUNC / NPR - A0 Resister divider 3
//Pos 5:  SD Card - Track 001 - A0 Resister divider 4
//Pos 6:  SD Card - Track 002 - A0 Resister divider 5
//Pos 7:  SD Card - Track 003 - A0 Resister divider 6
//Pos 8:  SD Card - Track 004 - A0 Resister divider 7


// Specifically for use with the Adafruit Feather, the pins are pre-set here!

// include SPI, MP3 and SD libraries
#include <SPI.h>
#include <SD.h>
#include <Adafruit_VS1053.h>
#include <ESP8266WiFi.h>
#include <FastLED.h>

WiFiClient client;

char *ssid = "EAJ";
const char *password = "emmaadamjono";

//RNZ (r)
const char *host = "radionz-ice.streamguys.com";
const char *path = "/national.mp3";
int httpPort = 80;

//WUNC World service (w)
const char *host2 = "edg-iad-wunc-ice.streamguys1.com";
const char *path2 = "/wunc-64-aac";
int httpPort2 = 80;

//BBC World service (b)
const char *host3 = "bbcwssc.ic.llnwd.net";
const char *path3 = "/stream/bbcwssc_mp1_ws-eieuk";
int httpPort3 = 80;

//Volume
int vol = 10;

//LED details 
#define NUM_LEDS_PER_STRIP 9  //Number of LEDs per strip  (count from 0)
#define PIN_LED 5            //I.O pin on ESP2866 device going to LEDs
#define COLOR_ORDER GRB       // LED stips aren't all in the same RGB order.  If colours are wrong change this  e.g  RBG > GRB.   :RBG=TARDIS
#define brightness2 255       //for stormy weather display

int green = 255;
int blue = 255;
int red = 255;

int LED_count = 0;          //For the loop

struct CRGB leds[NUM_LEDS_PER_STRIP]; //initiate FastLED with number of LEDs

// our little buffer of mp3 data
uint8_t mp3buff[32];   // vs1053 likes 32 bytes at a time
int radio = 0;         //1 means play radio in loop
//int loopcounter = 0;

// These are the pins used
#define VS1053_RESET   -1     // VS1053 reset pin (not used!)

#if defined(ESP8266)
  #define VS1053_CS      16      // VS1053 chip select pin (output)
  #define VS1053_DCS     15     // VS1053 Data/command select pin (output)
  #define CARDCS          4     // Card chip select pin
  #define VS1053_DREQ     0     // VS1053 Data request, ideally an Interrupt pin

// Feather ESP32
#elif defined(ESP32)
  #define VS1053_CS      32     // VS1053 chip select pin (output)
  #define VS1053_DCS     33     // VS1053 Data/command select pin (output)
  #define CARDCS         14     // Card chip select pin
  #define VS1053_DREQ    15     // VS1053 Data request, ideally an Interrupt pin

// Feather Teensy3
#elif defined(TEENSYDUINO)
  #define VS1053_CS       3     // VS1053 chip select pin (output)
  #define VS1053_DCS     10     // VS1053 Data/command select pin (output)
  #define CARDCS          8     // Card chip select pin
  #define VS1053_DREQ     4     // VS1053 Data request, ideally an Interrupt pin

// WICED feather
#elif defined(ARDUINO_STM32_FEATHER)
  #define VS1053_CS       PC7     // VS1053 chip select pin (output)
  #define VS1053_DCS      PB4     // VS1053 Data/command select pin (output)
  #define CARDCS          PC5     // Card chip select pin
  #define VS1053_DREQ     PA15    // VS1053 Data request, ideally an Interrupt pin

#elif defined(ARDUINO_NRF52832_FEATHER )
  #define VS1053_CS       30     // VS1053 chip select pin (output)
  #define VS1053_DCS      11     // VS1053 Data/command select pin (output)
  #define CARDCS          27     // Card chip select pin
  #define VS1053_DREQ     31     // VS1053 Data request, ideally an Interrupt pin

// Feather M4, M0, 328, nRF52840 or 32u4
#else
  #define VS1053_CS       6     // VS1053 chip select pin (output)
  #define VS1053_DCS     10     // VS1053 Data/command select pin (output)
  #define CARDCS          5     // Card chip select pin
  // DREQ should be an Int pin *if possible* (not possible on 32u4)
  #define VS1053_DREQ     9     // VS1053 Data request, ideally an Interrupt pin

#endif


Adafruit_VS1053_FilePlayer musicPlayer = 
  Adafruit_VS1053_FilePlayer(VS1053_RESET, VS1053_CS, VS1053_DCS, VS1053_DREQ, CARDCS);

void printDirectory(File dir, int numTabs);


void setup() {
  Serial.begin(115200);

  FastLED.addLeds<WS2811, PIN_LED, COLOR_ORDER>(leds, NUM_LEDS_PER_STRIP); //Initialise the LEDs


  // if you're using Bluefruit or LoRa/RFM Feather, disable the radio module
  //pinMode(8, INPUT_PULLUP);

  // Wait for serial port to be opened, remove this line for 'standalone' operation
  while (!Serial) { delay(1); }
  delay(500);
  Serial.println("\n\nAdafruit VS1053 Feather Test");
  
  if (! musicPlayer.begin()) { // initialise the music player
     Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));
     while (1);
  }

  Serial.println(F("VS1053 found"));
 
  musicPlayer.sineTest(0x44, 500);    // Make a tone to indicate VS1053 is working
  
  if (!SD.begin(CARDCS)) {
    Serial.println(F("SD failed, or not present"));
    while (1);  // don't do anything more
  }
  Serial.println("SD OK!");
  
  // list files
  printDirectory(SD.open("/"), 0);
  
  // Set volume for left, right channels. lower numbers == louder volume!
  musicPlayer.setVolume(vol, vol);
  
#if defined(__AVR_ATmega32U4__) 
  // Timer interrupts are not suggested, better to use DREQ interrupt!
  // but we don't have them on the 32u4 feather...
  musicPlayer.useInterrupt(VS1053_FILEPLAYER_TIMER0_INT); // timer int
#else
  // If DREQ is on an interrupt pin we can do background
  // audio playing
  musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);  // DREQ int
#endif

  //Wifi
  Serial.print("Connecting to SSID "); Serial.println(ssid);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
 
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");  Serial.println(WiFi.localIP());
  
  FastLED.setBrightness(255);
  
  fill_solid(&(leds[0]), NUM_LEDS_PER_STRIP /*number of leds*/, CRGB(0, 0, 0));
    FastLED.show();
  fill_solid(&(leds[0]), NUM_LEDS_PER_STRIP /*number of leds*/, CRGB(255, 0, 0));
    FastLED.show();
    delay (1000);
  fill_solid(&(leds[0]), NUM_LEDS_PER_STRIP /*number of leds*/, CRGB(0, 255, 0));
    FastLED.show();
    delay (1000);
  fill_solid(&(leds[0]), NUM_LEDS_PER_STRIP /*number of leds*/, CRGB(0, 0, 255));
    FastLED.show();  
    delay (1000);
    fill_solid(&(leds[0]), NUM_LEDS_PER_STRIP /*number of leds*/, CRGB(0, 0, 0));
    FastLED.show();
    delay (1000);
}

void loop() {

  yield();

  if (Serial.available()) {
    char c = Serial.read();

    if (c == '1') {

      musicPlayer.stopPlaying();          
      Serial.println(F("Playing track 001"));
      musicPlayer.startPlayingFile("/track001.mp3");
    }

    if (c == '2') {
      musicPlayer.stopPlaying();           
      Serial.println(F("Playing track 002"));
      musicPlayer.startPlayingFile("/track002.mp3");
    }

    // if we get an 's' on the serial console, stop!
    if (c == 's') {
            
      musicPlayer.stopPlaying();
      radio = 0;
      Serial.println("Stop");
    }

    //r=radio
    if (c == 'r') {
      radio = 1;    //make 1 in case r in pressed
      musicPlayer.stopPlaying();      
      Serial.println("Radio 1 on");

        /************************* INITIALIZE STREAM */
        Serial.print("connecting to ");  Serial.println(host);
        
        if (!client.connect(host, httpPort)) {
          Serial.println("Connection failed");
          return;
        }

        // We now create a URI for the request
        Serial.print("Requesting URL: ");
        Serial.println(path);
        
        // This will send the request to the server
        client.print(String("GET ") + path + " HTTP/1.1\r\n" +
                    "Host: " + host + "\r\n" + 
                    "Connection: close\r\n\r\n");
      }

    //r=radio
    if (c == 'w') {
      musicPlayer.stopPlaying();
      radio = 1;    //make 1 in case r in pressed
      Serial.println("Radio 2 on");

        /************************* INITIALIZE STREAM */
        Serial.print("connecting to ");  Serial.println(host2);
        
        if (!client.connect(host2, httpPort2)) {
          Serial.println("Connection failed");
          return;
        }

        // We now create a URI for the request
        Serial.print("Requesting URL: ");
        Serial.println(path2);
        
        // This will send the request to the server
        client.print(String("GET ") + path2 + " HTTP/1.1\r\n" +
                    "Host: " + host2 + "\r\n" + 
                    "Connection: close\r\n\r\n");
      }

    //r=radio
    if (c == 'b') {
      radio = 1;    //make 1 in case r in pressed
      musicPlayer.stopPlaying();
      Serial.println("Radio 3 on");

        /************************* INITIALIZE STREAM */
        Serial.print("connecting to ");  Serial.println(host3);
        
        if (!client.connect(host3, httpPort3)) {
          Serial.println("Connection failed");
          return;
        }

        // We now create a URI for the request
        Serial.print("Requesting URL: ");
        Serial.println(path3);
        
        // This will send the request to the server
        client.print(String("GET ") + path3 + " HTTP/1.1\r\n" +
                    "Host: " + host3 + "\r\n" + 
                    "Connection: close\r\n\r\n");
      }

    // if we get an 'p' on the serial console, pause/unpause!
    if (c == 'p') {
      
      if (! musicPlayer.paused()) {
        Serial.println("Paused");        
        musicPlayer.pausePlaying(true);
      } else { 
        Serial.println("Resumed");
        musicPlayer.pausePlaying(false);
      }
    }
  }

if (radio == 1) {
      yield();
      // wait till mp3 wants more data
      if (musicPlayer.readyForData()) {
        //Serial.print("ready ");

        //wants more data! check we have something available from the stream
        if (client.available() > 0) {
          yield();
          //Serial.print("set ");
          // yea! read up to 32 bytes
          uint8_t bytesread = client.read(mp3buff, 32);
          // push to mp3
          musicPlayer.playData(mp3buff, bytesread);
    
          //Serial.println("stream!");
        }
      }
        }


if (radio == 0) {
  yield();
  //LEDs
  fill_solid(&(leds[0]), NUM_LEDS_PER_STRIP /*number of leds*/, CRGB(0, 0, 0));
  fill_solid(&(leds[LED_count]), 1 /*number of leds*/, CRGB(red, green, blue));
  FastLED.setBrightness(255);
  FastLED.show();

  LED_count++;

  if (LED_count > NUM_LEDS_PER_STRIP){
    yield();
    LED_count = 0;
  }

  delay(100);
}

}


/// File listing helper
void printDirectory(File dir, int numTabs) {
   while(true) {
     
     File entry =  dir.openNextFile();
     if (! entry) {
       // no more files
       //Serial.println("**nomorefiles**");
       break;
     }
     for (uint8_t i=0; i<numTabs; i++) {
       Serial.print('\t');
     }
     Serial.print(entry.name());
     if (entry.isDirectory()) {
       Serial.println("/");
       printDirectory(entry, numTabs+1);
     } else {
       // files have sizes, directories do not
       Serial.print("\t\t");
       Serial.println(entry.size(), DEC);
     }
     entry.close();
   }
}