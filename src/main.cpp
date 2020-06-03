  
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
#include <BlynkSimpleEsp8266.h>
#define xstr(s) str(s)
#define str(s) #s
#define BLYNK_PRINT Serial
#define DBLYNKCERT_NAME "H3reyRYu6lEjFVRLbgwMF9JwLVMd8Lff"

const char auth[] = xstr(BLYNKCERT_NAME); // your BLYNK Cert from build flags
WiFiClient client;

//Gets SSID/PASSWORD from Platform.ini build flags
const char ssid[] = xstr(SSID_NAME);          //  your network SSID (name)
const char password[] = xstr(PASSWORD_NAME);      // your network password

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
int vol = 0;
int blynk_vol = vol;

//Breaks
const int pot_array_size = 10;
int pot_array_shim = 5;       //buffer between array values to stop shuttering between positions
//int pot_array[pot_array_size] = {0 - pot_array_shim - 1, 20, 250, 500, 720, 880, 1000, 1024 + pot_array_shim + 1};  //1024 exceeded due to shim value
int pot_array[pot_array_size] = {0 - pot_array_shim - 1, 20, 175, 300, 520, 680, 800, 900, 1000, 1024 + pot_array_shim + 1};  //1024 exceeded due to shim value
int pot_array_count = 0;      //counter to cycle through array
int new_pot_position = 0;      //For newly found position
int current_pot_position = 1000;  //currently held position to compare to a newly found position.   Start with 1000 so a new position will be found in the comparision with new position
unsigned long  current_pot_position_millis;  //millis for position
unsigned long  threshold = 1000;          //How long to be in the position before changing tracks
unsigned long  loop_millis;    //for delays
unsigned long  pause_millis;    //for delays
unsigned long  pause_delay = 750;    //for delays
int pause_flip = 0;    //for delays

long pot_check_millis;    //used for pot check delay
const int analogInPin = A0;  // ESP8266 Analog Pin ADC0 = A0
int new_sensorValue = 0;         // value read from the pot
unsigned long  pot_check_delay;
unsigned long  pot_check_delay_loop = 50;      //Blynk fails if 0, How long between checking pot, if radio is streaming it needs be be high otherwise faster in a loop
unsigned long  pot_check_delay_radio = 800;      //How long between checking pot, if radio is streaming it needs be be high
int pause = 0;
int blynk_pause = 0;
int blynk_menu = 0;
int menu = 0;
int trigger = 0;    //Used to trigger playing of stream/mp3s with Blynk.  -1 = No trigger from Blynk
int stopped = 0;    //1 when stopped and used to avoid blinking LED when paused

//LED details 
#define NUM_LEDS_PER_STRIP 9  //Number of LEDs per strip  (count from 0)
#define PIN_LED 5            //I.O pin on ESP2866 device going to LEDs
#define COLOR_ORDER GRB       // LED stips aren't all in the same RGB order.  If colours are wrong change this  e.g  RBG > GRB.   :RBG=TARDIS
#define brightness 32       //How bright are the LEDs

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
void check_pot_position();
void display_values();
void play_music();
void continue_stream();
void Blynk_check();

BLYNK_WRITE(V0) // Widget WRITEs to Virtual Pin
{
  blynk_vol = param.asInt(); // getting first value
}

BLYNK_WRITE(V1) // Widget WRITEs to Virtual Pin
{
  blynk_pause = param.asInt(); // getting first value
    pause_millis = millis();
}

BLYNK_WRITE(V2) // Widget WRITEs to Virtual Pin
{
  blynk_menu = param.asInt(); // getting first value  
}


void setup() {
  Serial.begin(115200);
  
  FastLED.addLeds<WS2811, PIN_LED, COLOR_ORDER>(leds, NUM_LEDS_PER_STRIP); //Initialise the LEDs

   // Wait for serial port to be opened, remove this line for 'standalone' operation
  while (!Serial) { delay(1); }
  delay(500);

  Blynk.config(auth);
  Blynk.connect();

  //VS1053 init
  Serial.println("\n\nAdafruit VS1053 Feather Test");
  
  if (! musicPlayer.begin()) { // initialise the music player
     Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));
     while (1);
  }
  Serial.println(F("VS1053 found"));
  
  musicPlayer.setVolume(50 - vol, 50 -vol);   // Set volume for left, right channels. lower numbers == louder volume!
  //musicPlayer.sineTest(0x44, 500);    // Make a tone to indicate VS1053 is working
  
  #if defined(__AVR_ATmega32U4__) 
    // Timer interrupts are not suggested, better to use DREQ interrupt!
    // but we don't have them on the 32u4 feather...
    musicPlayer.useInterrupt(VS1053_FILEPLAYER_TIMER0_INT); // timer int
  #else
    // If DREQ is on an interrupt pin we can do background
    // audio playing
    musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);  // DREQ int
  #endif


  //SD Card init
  if (!SD.begin(CARDCS)) {
    Serial.println(F("SD failed, or not present"));
    while (1);  // don't do anything more
  }
  Serial.println("SD OK!");
  printDirectory(SD.open("/"), 0);   // list files on SD


  //Wifi init
  Serial.print("Connecting to SSID "); Serial.println(ssid);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
 
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");  Serial.println(WiFi.localIP());


  //LED init 
  FastLED.setBrightness(brightness);
  
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

  //Set initial position millis count
  current_pot_position_millis = millis();
  pot_check_millis = millis();
  pot_check_delay = pot_check_delay_loop;
}



void loop() {
//Putting analgue read here causes stream to fail

Blynk.run(); 

Blynk_check();

if (trigger > 0){

  Serial.println("Blynk trigger");
  play_music();
}


//Check pot position if radio = 0 (!= 1)  or the delay is reached
//if (radio !=1 || millis() - pot_check_millis >= pot_check_delay){
if (millis() - pot_check_millis >= pot_check_delay){

//Serial.println("Pot check");
check_pot_position();
pot_check_millis = millis();
}

//if radio = 1 then I am streaming....keep pulling data
if (radio == 1 && pause == 0) {
  continue_stream();
}
 
if (pause == 1 && stopped == 0){
   
    if (millis() - pause_millis > pause_delay){
      FastLED.setBrightness(brightness * pause_flip);
      FastLED.show();

      if (pause_flip == 1){
        pause_flip = 0;
      }
      else
      {
        pause_flip =1;
      }
      pause_millis = millis();
    }
      }
            
  //display_values();  
}


//Play music based on pot_position
void play_music() {

if ((trigger == 0 && current_pot_position == 0) || trigger == 100) {
      musicPlayer.stopPlaying();
      
      loop_millis = millis();
      while (millis() - loop_millis < 500){
        yield();
        }

       Serial.println("Stop");
       radio = 0;         //1 means play radio in loop
       pot_check_delay = pot_check_delay_loop;

      fill_solid(&(leds[0]), NUM_LEDS_PER_STRIP /*number of leds*/, CRGB(0, 0, 0));
      fill_solid(&(leds[0]), 1 /*number of leds*/, CRGB(red, green, blue));
      FastLED.show();

       trigger = 0;   //Reset trigger so only fires this code once (like a change in pot value)
       stopped = 1;

       return;
  }

  if ((trigger == 0 && current_pot_position == 1) || trigger == 1) {
      musicPlayer.stopPlaying();
      radio = 1;    //make 1 if streaming radio playing
      pot_check_delay = pot_check_delay_radio;    
      
      loop_millis = millis();
      while (millis() - loop_millis < 500){
        yield();
        }

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

      loop_millis = millis();
      while (millis() - loop_millis < 500){
        yield();
        }

      fill_solid(&(leds[0]), NUM_LEDS_PER_STRIP /*number of leds*/, CRGB(0, 0, 0));
      fill_solid(&(leds[1]), 1 /*number of leds*/, CRGB(red, green, blue));
      FastLED.show();

      trigger = 0;   //Reset trigger so only fires this code once (like a change in pot value)
      stopped = 0;

      return;
      }


  if ((trigger == 0 && current_pot_position == 2) || trigger == 2) {

      musicPlayer.stopPlaying();
      radio = 1;    //make 1 if streaming radio playing
      pot_check_delay = pot_check_delay_radio;

      loop_millis = millis();
      while (millis() - loop_millis < 500){
        yield();
        }

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

      loop_millis = millis();
      while (millis() - loop_millis < 1000){
        yield();
        }

      fill_solid(&(leds[0]), NUM_LEDS_PER_STRIP /*number of leds*/, CRGB(0, 0, 0));
      fill_solid(&(leds[2]), 1 /*number of leds*/, CRGB(red, green, blue));
      FastLED.show();
      
      trigger = 0;   //Reset trigger so only fires this code once (like a change in pot value) 
      stopped = 0;   

      return;    
      }

  if ((trigger == 0 && current_pot_position == 3) || trigger == 3) {

      musicPlayer.stopPlaying();
      radio = 1;    //make 1 if streaming radio playing
      pot_check_delay = pot_check_delay_radio;
      
      loop_millis = millis();
      while (millis() - loop_millis < 500){
        yield();
        }

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

        loop_millis = millis();
        while (millis() - loop_millis < 1000){
          yield();
          }
      
      fill_solid(&(leds[0]), NUM_LEDS_PER_STRIP /*number of leds*/, CRGB(0, 0, 0));
      fill_solid(&(leds[3]), 1 /*number of leds*/, CRGB(red, green, blue));
      FastLED.show();

      trigger = 0;   //Reset trigger so only fires this code once (like a change in pot value)   
      stopped = 0;

      return;       
      }

  if ((trigger == 0 && current_pot_position == 4) || trigger == 4) {
      radio = 0;         //1 means play radio in loop
      pot_check_delay = pot_check_delay_loop;
      musicPlayer.stopPlaying();          

      loop_millis = millis();
      while (millis() - loop_millis < 500){
        yield();
        }

      Serial.println(F("Playing track 1"));
      musicPlayer.startPlayingFile("/1.mp3");

      loop_millis = millis();
      while (millis() - loop_millis < 1000){
        yield();
        }

      fill_solid(&(leds[0]), NUM_LEDS_PER_STRIP /*number of leds*/, CRGB(0, 0, 0));
      fill_solid(&(leds[4]), 1 /*number of leds*/, CRGB(red, green, blue));
      FastLED.show();

    trigger = 0;   //Reset trigger so only fires this code once (like a change in pot value)   
    stopped = 0;

    return;     
    }

  if ((trigger == 0 && current_pot_position == 5) || trigger == 5) {
      radio = 0;         //1 means play radio in loop
      pot_check_delay = pot_check_delay_loop;
      musicPlayer.stopPlaying();          
      
      loop_millis = millis();
      while (millis() - loop_millis < 500){
        yield();
        }

      Serial.println(F("Playing track 2"));
      musicPlayer.startPlayingFile("/2.mp3");
      
      loop_millis = millis();
      while (millis() - loop_millis < 1000){
        yield();
        }

      fill_solid(&(leds[0]), NUM_LEDS_PER_STRIP /*number of leds*/, CRGB(0, 0, 0));
      fill_solid(&(leds[5]), 1 /*number of leds*/, CRGB(red, green, blue));
      FastLED.show();

    trigger = 0;   //Reset trigger so only fires this code once (like a change in pot value)   
    stopped = 0;

    return;    
    }

  if ((trigger == 0 && current_pot_position == 6) || trigger == 6) {
      radio = 0;         //1 means play radio in loop
      pot_check_delay = pot_check_delay_loop;
      musicPlayer.stopPlaying(); 

      loop_millis = millis();
      while (millis() - loop_millis < 500){
        yield();
        }

      Serial.println(F("Playing track 3"));
      musicPlayer.startPlayingFile("/3.mp3");

      loop_millis = millis();
      while (millis() - loop_millis < 1000){
        yield();
        }

      fill_solid(&(leds[0]), NUM_LEDS_PER_STRIP /*number of leds*/, CRGB(0, 0, 0));
      fill_solid(&(leds[6]), 1 /*number of leds*/, CRGB(red, green, blue));
      FastLED.show();

    trigger = 0;   //Reset trigger so only fires this code once (like a change in pot value)   
    stopped = 0;

    return;      
    }  

  if ((trigger == 0 && current_pot_position == 7) || trigger == 7) {
      radio = 0;         //1 means play radio in loop
      pot_check_delay = pot_check_delay_loop;
      musicPlayer.stopPlaying(); 

      loop_millis = millis();
      while (millis() - loop_millis < 500){
        yield();
        }

      Serial.println(F("Playing track 4"));
      musicPlayer.startPlayingFile("/4.mp3");

      loop_millis = millis();
      while (millis() - loop_millis < 1000){
        yield();
        }

      fill_solid(&(leds[0]), NUM_LEDS_PER_STRIP /*number of leds*/, CRGB(0, 0, 0));
      fill_solid(&(leds[7]), 1 /*number of leds*/, CRGB(red, green, blue));
      FastLED.show();

    trigger = 0;   //Reset trigger so only fires this code once (like a change in pot value)   
    stopped = 0;

    return;        
    }  

  if ((trigger == 0 && current_pot_position == 8) || trigger == 8) {
      radio = 0;         //1 means play radio in loop
      pot_check_delay = pot_check_delay_loop;
      musicPlayer.stopPlaying(); 

      loop_millis = millis();
      while (millis() - loop_millis < 500){
        yield();
        }

      Serial.println(F("Playing track 5"));
      musicPlayer.startPlayingFile("/5.mp3");

      loop_millis = millis();
      while (millis() - loop_millis < 1000){
        yield();
        }

      fill_solid(&(leds[0]), NUM_LEDS_PER_STRIP /*number of leds*/, CRGB(0, 0, 0));
      fill_solid(&(leds[8]), 1 /*number of leds*/, CRGB(red, green, blue));
      FastLED.show();

    trigger = 0;   //Reset trigger so only fires this code once (like a change in pot value) 
    stopped = 0;

    return;          
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

void display_values(){
    //print the readings in the Serial Monitor
    Serial.print("sensor = ");
    Serial.print(new_sensorValue);
    Serial.print(",    pot_found = ");
    Serial.println(new_pot_position);

    Serial.print("radio = ");
    Serial.print(radio);
    Serial.print(",   current pot millis = ");
    Serial.print(current_pot_position_millis);
    Serial.print(",   current pot pos = ");
    Serial.print(current_pot_position);
    Serial.print(",   pot pos = ");
    Serial.println(current_pot_position);
}

void continue_stream(){
          //Serial.print(".");  
        //yield();
        // wait till mp3 wants more data
        if (musicPlayer.readyForData()) {
          //Serial.print("ready ");

          //wants more data! check we have something available from the stream
          if (client.available() > 0) {
            //yield();
            //Serial.print("set ");
            // yea! read up to 32 bytes
            uint8_t bytesread = client.read(mp3buff, 32);
            // push to mp3
            musicPlayer.playData(mp3buff, bytesread);
      
            //Serial.println("stream!");
          }
        }
}

void check_pot_position(){

  //old_sensorValue = new_sensorValue;
  new_sensorValue = analogRead(analogInPin);

  //Figure out position based on Pot_array[]
  pot_array_count = 0;   //counter to cycle through array
  new_pot_position = -1;   //-1 means nothing found by default (e.g not a valid position)

  while (new_pot_position == -1){

    if (new_sensorValue >= (pot_array[pot_array_count] + pot_array_shim) && new_sensorValue < (pot_array[pot_array_count + 1] - pot_array_shim)){
      new_pot_position = pot_array_count;
      break;
    }

    pot_array_count++;

    //No position found, probably in between positions due to shim value.  In this case keep current position
    if (pot_array_count > pot_array_size -1){
        new_pot_position = current_pot_position;    //if true, keep current position
        break;
    }
  }


  //new position, start millis count and set current position unless in -1 error state
  if (new_pot_position != current_pot_position && new_pot_position != -1){
      
      current_pot_position = new_pot_position;
      current_pot_position_millis = millis();  
      pot_check_delay = pot_check_delay_loop;
      radio = 0;   //It's moved, stop streaming

      }
    
  //New position, check how long it's been in this new position
  //If current_pot_postion_millis > 0 then we assume a new position has been reached 
  if (current_pot_position_millis != 0){

    //LEDs
    fill_solid(&(leds[0]), NUM_LEDS_PER_STRIP /*number of leds*/, CRGB(0, 0, 0));
    fill_solid(&(leds[current_pot_position]), 1 /*number of leds*/, CRGB(red, green, blue));
      FastLED.show();

    if (millis() - current_pot_position_millis > threshold) {
        
        //We have gone over theshold, do the mp3 and stop this main 'if' from passing
        //current_pot_postion_millis only greater than 0 when new position found
        current_pot_position_millis = 0;
        Serial.println();
        Serial.print("Threshold reach = ");
        Serial.print(millis());

        //LEDs
        fill_solid(&(leds[0]), NUM_LEDS_PER_STRIP /*number of leds*/, CRGB(0, 0, 0));
        fill_solid(&(leds[current_pot_position]), 1 /*number of leds*/, CRGB(red, green, blue));
          FastLED.show();

        Serial.print("*** current_pot_position ***  = ");
        Serial.println(current_pot_position);

        play_music();

      }  
  }  
}


void Blynk_check(){
  if (blynk_vol != vol){
  vol = blynk_vol;
  musicPlayer.setVolume(50 - vol, 50 - vol);   // Set volume for left, right channels. lower numbers == louder volume!
  //Serial.println(vol);
  //Serial.println(blynk_vol);
  }

 if (pause != blynk_pause) {

      if (! musicPlayer.paused()) {
        musicPlayer.pausePlaying(true);
              FastLED.setBrightness(brightness);
              FastLED.show();
      } else { 
        musicPlayer.pausePlaying(false);
              FastLED.setBrightness(brightness);
              FastLED.show();
      }
  pause = blynk_pause;
  }

if (menu != blynk_menu){
menu = blynk_menu;

switch (blynk_menu)
  {
    case 1: { // Item 1
      Serial.println("Stop");
      trigger = 100;
      break;
    }
    case 2: { // Item 2
      Serial.println("Radio NZ");
      trigger = 1;
      break;
    }
    case 3: { // Item 3
      Serial.println("WUNC");
      trigger = 2;      
      break;
    }
    case 4: { // Item 4
      Serial.println("BBC World service");
      trigger = 3;      
      break;
    }    
    case 5: { // Item 5
      Serial.println("MP3_1");
      trigger = 4;      
      break;
    }    
    case 6: { // Item 6
      Serial.println("MP3_2");
      trigger = 5;      
      break;
    }  
    case 7: { // Item 7
      Serial.println("MP3_3");
      trigger = 6;      
      break;
    }  
    case 8: { // Item 8
      Serial.println("MP3_4");
      trigger = 7;      
      break;
    }
    case 9: { // Item 9
      Serial.println("MP3_5");
      trigger = 8;
      break;
    }                 
  }
}
}