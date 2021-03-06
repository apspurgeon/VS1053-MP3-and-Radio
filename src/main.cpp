  
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

int NumberofRadiostations = 3;

const char host[][40] = 
{
  "radionz-ice.streamguys.com",   //Radio staion 1
  "edg-iad-wunc-ice.streamguys1.com",   //Radio staion 2
  "bbcwssc.ic.llnwd.net"   //Radio staion 3
};

const char path[][30] = 
{
  "/national.mp3",   //Radio staion 1
  "/wunc-64-aac",   //Radio staion 2
  "/stream/bbcwssc_mp1_ws-eieuk"   //Radio staion 3
};

const char httpPort[][5] = 
{
  "80",   //Radio staion 1
  "80",   //Radio staion 2
  "80"   //Radio staion 3
};


char mp3_file[][70] = // 10 is the length of the longest string + 1 ( for the '\0' at the end )
{
	"/BBC Radio WWII 1of4.mp3",
	"/BBC Radio WWII 2of4.mp3",
	"/BBC Radio WWII 3of4.mp3",
  "/BBC Radio WWII 4of4.mp3",
  "/Orson Welles - War Of The Worlds 1938.mp3",
  "/HM King George VI - War outbreak speech - 1939.mp3",
  "/Lord Haw Haw - British Invasion Looms 1940.mp3",
  "/Lord Haw Haw - Germany Calling 1941.mp3",
  "/Germany Calling William Joyces Last British Broadcast.mp3",
};


//Volume (0=Loudest 50=Quietest).  Flipped in the code so slider in Blynk is low to high (left to right)
int vol = 0;
int blynk_vol = vol;

const int pot_array_size = 10;
int pot_array_shim = 5;           //buffer between array values to stop shuttering between positions
int pot_array[pot_array_size] = {0 - pot_array_shim - 1, 20, 175, 300, 520, 680, 800, 900, 1000, 1024 + pot_array_shim + 1};  //1024 exceeded due to shim value
int pot_array_count = 0;          //counter to cycle through array
int new_pot_position = 0;         //For newly found position
int current_pot_position = 1000;  //Currently held position to compare to a newly found position.   Start with 1000 so a new position will be found in the comparision with new position

unsigned long  current_pot_position_millis;  //millis for position
unsigned long  threshold = 1000;             //How long to be in the position before changing tracks
unsigned long  loop_millis;                  //for needed delays instead of delay()

long pot_check_millis;                        //used for pot check delay
unsigned long  pot_check_delay;
unsigned long  pot_check_delay_loop = 50;     //Blynk fails if 0, How long between checking pot, if radio is streaming it needs be be high otherwise faster in a loop
unsigned long  pot_check_delay_radio = 800;   //How long between checking pot, if radio is streaming it needs be be high

unsigned long  pause_millis;                 //for Pause LED on/off count
unsigned long  pause_delay = 750;            //How long for LED on/off cycle during pause
int pause = 0;                               //current pause 0=no pause, 1=paused
int blynk_pause = 0;                         //pause from blynk to compare against current pause
int pause_flip = 0;                          //for pause LED on/off flips

const int analogInPin = A0;       //ESP8266 Analog Pin ADC0 = A0 for the Pot
int new_sensorValue = 0;         // value read from the pot

int menu = 0;             //current menu position
int blynk_menu = 0;       //Blynk menu position, compare to current menu position
int Blynk_trigger = 0;          //Used to Blynk_trigger playing of stream/mp3s with Blynk.  0 = No Blynk_trigger from Blynk
int stopped = 0;          //1 when stopped and used to avoid blinking LED when paused pressed
int to_play;              //Which mp3 to play

//LED details 
#define NUM_LEDS_PER_STRIP 9  //Number of LEDs per strip  (count from 0)
#define PIN_LED 5            //I.O pin on ESP2866 device going to LEDs
#define COLOR_ORDER GRB       // LED stips aren't all in the same RGB order.  If colours are wrong change this  e.g  RBG > GRB.   :RBG=TARDIS
int brightness = 10;       //How bright are the LEDs.  Low is good, WS2812 are noisy at higher brightness
int blynk_brightness = brightness;

//White
int green = 255;
int blue = 255;
int red = 255;

struct CRGB leds[NUM_LEDS_PER_STRIP]; //initiate FastLED with number of LEDs

// our little buffer of mp3 data
uint8_t mp3buff[32];   // vs1053 likes 32 bytes at a time
int radio = 0;         //1 means play radio in loop

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


//Volume from Blynk
BLYNK_WRITE(V0) // Widget WRITEs to Virtual Pin
{
  blynk_vol = param.asInt(); // getting first value
}

//Puase from Blynk
BLYNK_WRITE(V1) // Widget WRITEs to Virtual Pin
{
  blynk_pause = param.asInt(); // getting first value
    pause_millis = millis();
}

//Menu from Blynk
BLYNK_WRITE(V2) // Widget WRITEs to Virtual Pin
{
  blynk_menu = param.asInt(); // getting first value  
}

//LED Brightness from Blynk
BLYNK_WRITE(V3) // Widget WRITEs to Virtual Pin
{
  blynk_brightness = param.asInt(); // getting first value  
}

void printDirectory(File dir, int numTabs);
void check_pot_position();
void display_values();
void play_music();
void continue_stream();
void Blynk_check();


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

if (Blynk_trigger > 0){

  Serial.println("Blynk_trigger");
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

red = 255;
blue = 255;
green = 255;

//Check iseither the Blynk_trigger (100) or current pot position (0) are saying STOP
if ((Blynk_trigger == 0 && current_pot_position == 0) || Blynk_trigger == 100) {
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

       Blynk_trigger = 0;   //Reset Blynk_trigger so only fires this code once (like a change in pot value)
       stopped = 1;

       return;
  }

  //Greater than 0 (stop) and less or equal to the number of radio stations (3)
  if ((Blynk_trigger == 0 && (current_pot_position > 0 && current_pot_position <= NumberofRadiostations)) || (Blynk_trigger > 0 && Blynk_trigger <= NumberofRadiostations)) {

  //I need to play something (condition above passed), work out what
  if ((Blynk_trigger > 0 && Blynk_trigger <=NumberofRadiostations) || (current_pot_position > 0 && current_pot_position <= NumberofRadiostations)){
    if (Blynk_trigger == 0 && current_pot_position <= NumberofRadiostations)  {   //Not a Blynk Blynk_trigger and within the 3 radio stations
      to_play = current_pot_position -1;   //should be 1-3, and from the pot
      }
      else
      {
      to_play = Blynk_trigger -1;
      }

      //convert the httpPort array into an int
      String httpPort_use_String = httpPort[to_play];
      int httpPort_use = httpPort_use_String.toInt();
 
      musicPlayer.stopPlaying();
      radio = 1;    //make 1 if streaming radio playing
      pot_check_delay = pot_check_delay_radio;    
      
      loop_millis = millis();
      while (millis() - loop_millis < 500){
        yield();
        }

      Serial.println("Radio station");

        /************************* INITIALIZE STREAM */
        Serial.print("connecting to ");  Serial.println(host[to_play]);
        
        if (!client.connect(host[to_play], httpPort_use)) {
          Serial.println("Connection failed");
          return;
        }

        // We now create a URI for the request
        Serial.print("Requesting URL: ");
        Serial.println(path[to_play]);
        
        // This will send the request to the server
        client.print(String("GET ") + path[to_play] + " HTTP/1.1\r\n" +
                    "Host: " + host[to_play] + "\r\n" + 
                    "Connection: close\r\n\r\n");

      loop_millis = millis();
      while (millis() - loop_millis < 500){
        yield();
        }

      fill_solid(&(leds[0]), NUM_LEDS_PER_STRIP /*number of leds*/, CRGB(0, 0, 0));
      fill_solid(&(leds[to_play + 1]), 1 /*number of leds*/, CRGB(red, green, blue));
      FastLED.show();

      Blynk_trigger = 0;   //Reset Blynk_trigger so only fires this code once (like a change in pot value)
      stopped = 0;

      return;
      }
  }


  //Play MP3 on SD card, selection is greater than the number of radio stations
  if (Blynk_trigger > NumberofRadiostations || current_pot_position > NumberofRadiostations){
    if (Blynk_trigger == 0 && current_pot_position > NumberofRadiostations)  {   //Not a Blynk Blynk_trigger and more than the number of radio stations
      to_play = current_pot_position;
      }
      else
      {
      to_play = Blynk_trigger;
      }

  if ((Blynk_trigger == 0 && current_pot_position == to_play) || Blynk_trigger == to_play) {

      Serial.print("Play MP3 - ");
      Serial.println(mp3_file[to_play -(NumberofRadiostations + 1)]);

      radio = 0;         //1 means play radio in loop, 0 means an SD Card MP3 (don't stream)
      pot_check_delay = pot_check_delay_loop;
      musicPlayer.stopPlaying();          

      loop_millis = millis();
      while (millis() - loop_millis < 500){
        yield();
        }

      musicPlayer.startPlayingFile(mp3_file[to_play -(NumberofRadiostations + 1)]);

      loop_millis = millis();
      while (millis() - loop_millis < 1000){
        yield();
        }

      fill_solid(&(leds[0]), NUM_LEDS_PER_STRIP /*number of leds*/, CRGB(0, 0, 0));

      //If more than the number of LEDs change the colour to show "looping around".  Only works with Blynk
      //Yellow when looped
      if (to_play > NUM_LEDS_PER_STRIP -1){
        red = 255;
        blue = 0;
        green = 255;
      }

      //If more than the number of LEDs loop around.  Only works with Blynk
      if (to_play > NUM_LEDS_PER_STRIP -1){
      fill_solid(&(leds[to_play- NUM_LEDS_PER_STRIP]), 1 /*number of leds*/, CRGB(red, green, blue));
      }
      else
      {      
      fill_solid(&(leds[to_play]), 1 /*number of leds*/, CRGB(red, green, blue));
      }

      FastLED.show();

    Blynk_trigger = 0;   //Reset Blynk_trigger so only fires this code once (like a change in pot value)   
    stopped = 0;

    return;     
    }
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

        // wait till mp3 wants more data
        if (musicPlayer.readyForData()) {
          //wants more data! check we have something available from the stream
          if (client.available() > 0) {
            //read up to 32 bytes
            uint8_t bytesread = client.read(mp3buff, 32);
            // push to mp3
            musicPlayer.playData(mp3buff, bytesread);
          }
        }
}

void check_pot_position(){

  new_sensorValue = analogRead(analogInPin);

  //Figure out position based on Pot_array[]
  pot_array_count = 0;     //counter to cycle through array
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
      radio = 0;      //It's moved, stop streaming

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
  }


if (blynk_brightness != brightness){
  brightness = blynk_brightness;
    FastLED.setBrightness(brightness);
    FastLED.show();
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

if (blynk_menu == 1){
        Serial.println("Stop");
      Blynk_trigger = 100;
}

if (blynk_menu == 2){
      Serial.println("Radio NZ");
      Blynk_trigger = 1;
}

if (blynk_menu == 3){
      Serial.println("WUNC");
      Blynk_trigger = 2;      
}

if (blynk_menu == 4){
      Serial.println("BBC World service");
      Blynk_trigger = 3;      
}

if (blynk_menu >4){
      Serial.print("MP3 Track ");
      Serial.println(blynk_menu - 4);
      Blynk_trigger = blynk_menu -1;      
}
}
}