#include <WiFi.h>
#include "ArduinoJson.h"
#include "time.h"
#include <HTTPClient.h>
#include "JSONRequest.h"
#include "FS.h"
#include "dogClass.h"
#include <LittleFS.h>

#define FORMAT_LITTLEFS_IF_FAILED true
const char * cfgPath = "/cfg.txt";
//#include <ArduinoJson.hpp>

// Include image arrays
#include "smallDogs.h"
#include "smallBg.h"
#include "emoji.h"
#include "bubble.h"
#include "timeout.h"
#include "button.h"

// Include the TFT library - see https://github.com/Bodmer/TFT_eSPI for library information
#include "SPI.h"
#include <TFT_eSPI.h>              // Hardware-specific library
TFT_eSPI tft = TFT_eSPI();

///// Configurable options //////////////
String openWeatherMapKey = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"; //you'll need to get this app key from https://openweathermap.org/api_keys

// default configuration file; only used if the config.txt file hasn't been created. 
// Add your wifi SSID and password into the respective arrays at the same index in each
// For CarletonGuests, you'll need to get the board mac address to add it manually through the netreg process
char cfgJson[] = "{ \
  \"password\":[\"\"],\
  \"ssid\":[\"CarletonGuests\"],\
  \"clock\":true\,\
  \"zipcode\":\"55057\"\
}";


// Enum to store the menu state
enum Menu {
  HIDDEN,
  MAIN,
  SETTINGS,
  WIFI
};

//TFT sprites to render the background and sprites
TFT_eSprite bgSpr = TFT_eSprite(&tft);
TFT_eSprite spr = TFT_eSprite(&tft);
TFT_eSprite dogSpr = TFT_eSprite(&tft);
TFT_eSprite emojiSpr = TFT_eSprite(&tft);
TFT_eSprite bubbleSpr = TFT_eSprite(&tft);

//physical button declarations
Button leftBut;
Button midBut;
Button rightBut;

//Temp string to store keyboard content
String kbString = "";
char kbInd = 65;  //declaration for current character pointer

//longs to store time for menu related functions.
unsigned long backPressTimer = 0;
unsigned long menuPressTimer = 0;

bool watchBackPress = false;
bool watchMenuPress = false;

//bools to selectively ignore release of the buttons
bool ignoreSelectRelease = false;
bool ignoreBackRelease = false;

int menuLevel = 0; //var to store the current menu level
unsigned long menuTimeout = 0; //timer to dismiss the menu after inactivity


Menu menu = HIDDEN; //current menu mode flag, not used?
bool bMenu = false; //bool storing whether the menu is open or not

//Strings for toggle options
String toggleChoices[] = {"OFF", "ON"};

//index for toggle options, probably should be a bool
int toggleInd = 0;

//vars for dealing with listing SSIDs
int ssidInd = 0; //index for currently displayed SSID
int numSsids = 0; //total number of SSIDs found
int curSSID = 0; //index for saved SSID

//variables for storing info about current time and temp
String tempString = "";
int timezoneOffset = 0;
unsigned long sunrise = 0;
unsigned long sunset = 0;
bool daytime = true;

/*///////////////////////////////////////////////////////////////////////
The menu is entirely stored in menuDoc as JSON nodes.
Each menu option has at least a name and an icon number. Menu options that have submenus have two more keys, "opts", and "ind".
"ind" stores the integer index of what the currently selected option is for that submenu, and "opts" store all of the submenu options.
Some options also have a "type" key, which establishes behaviour, such as being a toggle or a keyboard input

level "main" has no icon number, as it is not selectable as an option
////////////////////////////////////////////////////////////////////////*/
char menuJson[] = "{\"name\":\"main\",\"opts\":[\
  {\"name\":\"feed\",\"icon\":4},\
  {\"name\":\"play\",\"icon\":5},\
  {\"name\":\"poop\",\"icon\":1},\
  {\"name\":\"settings\",\"icon\":8, \"opts\":[\
    {\"name\":\"clock\", \"icon\":11, \"type\":\"toggle\"},\
    {\"name\":\"weather\", \"icon\":12, \"type\":\"kb\",\"range\":[48,57,48]},\
    {\"name\":\"wifi\",\"icon\":10, \"opts\":[ \
      {\"name\":\"mac\",\"type\":\"display\"},\
      {\"name\":\"ssid\",\"type\":\"display\"},\
      {\"name\":\"password\",\"type\":\"kb\",\"range\":[33,126,65]}\
    ], \"ind\":0}\
  ], \"ind\":0}\
], \"ind\":0, \"level\":0, \"path\":[0,0,0]}";

//declare the JSON documents for configuration and menu. Probably should be static, rather than dynamic,
// but the chips have enough ram that I don't care.
DynamicJsonDocument menuDoc(1024);
DynamicJsonDocument cfgDoc(1024);
JsonObject level; //JSONObject reference to store the current menu level.

//vars storing info about the ballchase
float ballTmr = 0;
bool ballChase = 0;
bool hasBall = 0;
  
Dog dog; //create our dog instance.


//function to select the next menu level down.
void selectLevel(){
  menuLevel++; //increment the level pointer
  level = level["opts"][level["ind"].as<int>()]; //set the level to the currently displayed option
  Serial.println(level["name"].as<String>()); // and print
}

//function to go to the previous menu level
void previousLevel(){
  level = menuDoc.as<JsonObject>(); //use the menu document as the base level, 
  for(int i = 0; i<menuLevel-2; i++){ // and step down into menuLevel-2 submenu levels. Since each submenu stores the index of the last option selected, it will take you to the proper option.
    level = level["opts"][level["ind"].as<int>()];
  }
  menuLevel--; //decrement the menu level index.
  Serial.println(level["name"].as<String>());
}

// print text into the menu area at a specified size.
void menuText(String prnt, int size = 2){
  spr.setTextSize(size); //set the text size
  int twid = spr.textWidth(prnt); // and get the width of the printed text
  spr.setCursor(120 - twid/2, 240-32); //set the cursor so the text is centered
  spr.setTextColor(TFT_WHITE); //set the color
  spr.println(prnt); // and print.
}
 
//write the config file to the filesystem in flash memory.
void writeConfig(){
  File file = LittleFS.open(cfgPath, FILE_WRITE); //open the file path
  if(!file){
      Serial.println("- failed to open file for writing");
      return;
  }
  serializeJsonPretty(cfgDoc,file); //and serialize the JSON file into the opened file.
  Serial.println("Wrote config.");
  file.close(); //make sure to close the file when done.
}

//import the config file from the flash.
void readConfig(){
  File file = LittleFS.open(cfgPath); //open the config file
  if(!file || file.isDirectory()){ //if the file doesn't exist yet,
      Serial.println("File does not exist, creating file from default.");
      deserializeJson(cfgDoc, cfgJson); //load the default config string
      writeConfig(); //and write it to the file.
  } else { //otherwise,
    deserializeJson(cfgDoc, file); //parse the JSON in the file to a usable state.
    file.close(); // and close the file. 
  }
  
}

//function to update the current temperature
void updateWeather(){
  //make a webrequest to openweather map; you'll need to get your own key, and change it in the code above.
  web.request("http://api.openweathermap.org/data/2.5/weather?zip="+cfgDoc["zipcode"].as<String>()+",us&units=imperial&appid="+openWeatherMapKey, [](DynamicJsonDocument&doc){
      tempString = String(doc["main"]["temp"].as<int>()); //get the temp as an integer.
      timezoneOffset = doc["timezone"].as<int>(); // and pull out the local timezone and sunrise and sunset times.
      sunrise = doc["sys"]["sunrise"].as<unsigned long>();
      sunset = doc["sys"]["sunset"].as<unsigned long>();
      time_t now;
      time(&now);
      daytime = (now > sunrise && now < sunset); //check if it's daytime.
  });
  setTimeout(updateWeather,600000); //check the weather again in ten minutes.
}

//====================================================================================
//                                    Setup
//====================================================================================
void setup()
{
  Serial.begin(115200); //begin serial output, for debugging
  delay(2000); //and wait for a couple seconds to boot up properly

  Serial.println("------------------");
  Serial.println(ESP.getFreeHeap()); //print the free memory

  if(!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)){ //check if littleFS inits properly
      Serial.println("LittleFS Mount Failed");
      return;
  }

  readConfig(); // read in the stored config file

  deserializeJson(menuDoc, menuJson); // parse the menu string from the top of this file. 

  //serializeJson(cfgDoc, Serial); // print the config file to the serial 

  // Initialise the TFT
  tft.init();  // start communication with the display
  dogSpr.setColorDepth(8); //minimize the size of the sprite by setting an 8 bit color palette
  bgSpr.setColorDepth(8);
  spr.setColorDepth(8);

  //allocate memory for each of the sprites
  dogSpr.createSprite(32,32); //1/4 resolution dog
  bubbleSpr.createSprite(32,32);
  emojiSpr.createSprite(16,16);
  bgSpr.createSprite(120,120); //half resolution background
  spr.createSprite(240,240); //full screen size frame buffer

  bgSpr.pushImage(0, 0, SMALLBG_WIDTH, SMALLBG_HEIGHT, smallBg); //push the bgImage into the bgSpr, so we can use it to make day/night
  bubbleSpr.pushImage(0,0, BUBBLE_WIDTH, BUBBLE_HEIGHT, bubble); //store the bubble into a sprite

  scale(&bgSpr,&spr,0,0,2,false); //push a 2x scaled up version of the background into the frame buffer

  //draw a half opacity black box over the middle of the image.
  for(int i=0; i<55; i++){
    for(int j=0; j<spr.width(); j++){
      uint32_t col = spr.readPixel(j,i+90);
      col = fastBlend(128, col, TFT_BLACK);
      spr.drawPixel(j, i+90, col);
    }
  }

  //set the text size and center the text for the splash screen.
  spr.setTextSize(3);
  int twid = spr.textWidth("carl-o-gotchi");
  spr.setCursor(120 - twid/2, 110);
  spr.setTextColor(TFT_WHITE);
  spr.print("wade-o-gotchi");

  //push the frame buffer onto the screen
  spr.pushSprite(0,0);

  String which = "";
  String pword = "";
  bool ssidFound = false;

  //find a local network that matches the name of an SSID in the config file.
  numSsids = WiFi.scanNetworks();
  for(int i=0; i<numSsids; i++){ //for all of the found SSIDs
    for(int j=0; j<cfgDoc["ssid"].size(); j++){ //compare them to the ssids in the config
      if(cfgDoc["ssid"][j].as<String>() == String(WiFi.SSID(i))){ //if we find a match
        ssidFound = true; //mark as found
        which = cfgDoc["ssid"][j].as<String>(); //and store the matching ssid and password
        pword = cfgDoc["password"][j].as<String>();
        curSSID = j; //and store the index of the matching config SSID
        break;
      }
    }
  }

  if(ssidFound) WiFi.begin(which,pword); //if we found a matching local SSID, try to connect.

  int tryCount = 0; // variable to keep track of connection attempt time.

  while (WiFi.status() != WL_CONNECTED && tryCount < 20 && ssidFound) { //while we're not connected, or for 10 seconds (if an SSID was found)
    delay(500); //wait.
    tryCount++;
    Serial.print(".");
  }
  

  if(WiFi.status() == WL_CONNECTED){ //if we connected before the timeout...
    Serial.println(" CONNECTED");
    updateWeather();
    configTime(timezoneOffset, 0, "pool.ntp.org", "time.nist.gov"); //configure the current time to the timeserver

    time_t now;
    while(now < 1703027354){ //and wait for it to be a unix time later than december 19th, 2023
      time(&now);
    }

    updateWeather(); //once the time is correct, get the current weather
    

    Serial.print("Current time is: ");
    Serial.println(now);
    Serial.print("Sunset is ");
    Serial.println(sunset);
    daytime = (now > sunrise && now < sunset); //update the daytime status.
  } else {
    Serial.println("TIMED OUT or NOT FOUND");
  }

  Serial.println("------------------");
  Serial.println(ESP.getFreeHeap()); //print the available heap, again.

  pinMode(12,INPUT_PULLDOWN); //set pin12 to an input, because it's directly connected to GND, and don't want to fry it accidently.

/////////////////////////////////////////////////////////////////////////////
// LEFT BUTTON SETUP
/////////////////////////////////////////////////////////////////////////////

  leftBut.setup(35,[](int state){
    menuTimeout = millis()+30000; //reset the menu timeout to 30 seconds from now.
    if(state){ //if the button was just pressed...
      backPressTimer = millis();
      if(level["type"] == "kb"){ //and the current level is a keyboard select level, 
        watchBackPress = true;
      } else if(menuLevel>1){ //otherwise, if the level isn't a keyboard, but we're in a submenu
        previousLevel(); //just take us to the previous level.
      } else { //otherwise, if we in the main menu,
        bMenu = false; //close the menu
        menuLevel = 0;
      }
    } else { //if we just release the back button
      if(!ignoreBackRelease && watchBackPress && level["type"] == "kb"){ //and it is a keyboard level,
        if(millis()-backPressTimer < 1000) kbString.remove(kbString.length()-1); //and it hasn't been a second since with pressed the button, just delete the last character in the input. 
        //else previousLevel();
      }
      ignoreBackRelease = false;
      watchBackPress = false;
    }
  },20); //20 is the debounce time for the button; any press shorter than 20ms won't be counted.

////////////////////////////////////////////////////////////////////////////
// MIDDLE BUTTON SETUP
////////////////////////////////////////////////////////////////////////

  midBut.setup(16,[](int state){
    menuTimeout = millis()+30000; //keep the menu open for 30 seconds longer
    if(state){ //if we pressed the button
      menuPressTimer = millis(); //store the current time in the menuPress timer
      if(bMenu){ //if the menu is already open
        JsonObject opt = level["opts"][level["ind"].as<int>()].as<JsonObject>(); //store the current option in opt.
        if(opt && opt.containsKey("opts")){ //if opt is a submenu
          selectLevel(); //step into the submenu
          level["ind"] = 0; //and reset the selection index for that submenu.
        } else if(opt && opt.containsKey("type")){ //if the option has a type key (ie, it displays more info or has an input option)
          if(opt["type"] == "kb") kbInd = opt["range"][2].as<int>(); //if it's a keyboard, set the currently displayed character to the options default 

          //Set options according to the name of the option
          if(opt["name"] == "password") kbString = cfgDoc["password"][curSSID].as<String>(); //if it is the password option, set the keyboard input to the current password
          else if(opt["name"] == "weather") kbString = cfgDoc["zipcode"].as<String>(); //if it's the weather setting, set the keyboard input to the current zipcode from config.
          else if(opt["name"] == "clock") toggleInd = cfgDoc["clock"].as<bool>(); //if it's the clock config, set it to the current showing state of the clock
          else if(opt["name"] == "ssid"){ //if it's the SSID option, scan for new wifi hotspots.
            spr.fillRect(0, 240-36, 240, 36, TFT_BLACK); //make a black rectangle in the frame buffer,
            menuText("Scanning...",2); //write text onto it.
            spr.pushSprite(0,0);  //and print it to the screen, so there's feedback about why everything froze. 
            numSsids = WiFi.scanNetworks();
          }
          selectLevel(); //step into the new option.
          ignoreSelectRelease = true; //ignore the release as we step into the next level.
        } else if(level.containsKey("type")){ //if the current level has a type key.
          if(level["name"] == "clock") cfgDoc["clock"] = toggleInd, writeConfig(); //save the current index into the config, and write the config
          else if(level["name"] == "ssid"){ //if the current level is the SSID,
            String newSSID = WiFi.SSID(ssidInd); //get the name of the new SSID
            bool found = false; //bool to store whether we found the new ssid in the config 
            Serial.println(newSSID);
            for(int i=0; i<cfgDoc["ssid"].size(); i++){
              if(cfgDoc["ssid"][i].as<String>() == newSSID){
                curSSID = i;
                found = true; //if we find it, store the index in curSSID
                break;
              }
            }
            if(!found){ //if we didn't find it, store it.
              curSSID = cfgDoc["ssid"].size();
              cfgDoc["ssid"][curSSID] = newSSID;
              writeConfig();
            }
          } else if(level["type"] == "kb") watchMenuPress = true;
        } else { //otherwise, let's just do the actions the option wants. Maybe self explanatory.
          String name = level["opts"][level["ind"].as<int>()]["name"]; //Get the name of the opt.
          Serial.println(name);
          if(name=="feed"){
            dog.giveFood();
          } else if(name == "play"){
            ballTmr = millis() + 1000;
            ballChase = true;
            dog.stepTime = 150;
            hasBall = false;
          } else if(name == "poop"){
            curPoops = 0;
            if(dog.note == POOP) dog.note = HEART;
            dog.free();
          }

        }
      } else { //if the menu isn't open, open it. 
        bMenu = true;
        menuLevel = 1;
        level = menuDoc.as<JsonObject>();
      }
    } else if(!ignoreSelectRelease){ //we just relased, and aren't ignoring it.
      long pressTime = millis() - menuPressTimer; //find out how long it's been since it was pressed.
      if(bMenu && watchMenuPress){ //if the menu is open...
        if(level.containsKey("type")){ //and it has a type option
          if(level["type"] == "kb"){ // and it is a kb
            if(pressTime < 1000) kbString+=kbInd; //if the press time is less than a second, just add the next character
            watchMenuPress = false;
          }
           
        }
      }
    } else {
      ignoreSelectRelease = false; //reset the selectRelease flag
      watchMenuPress = false;
    }
  },20); //20 is the debounce time for the button; any press shorter than 20ms won't be counted.

////////////////////////////////////////////////////////////////////////////
// RIGHT BUTTON SETUP
////////////////////////////////////////////////////////////////////////

  rightBut.setup(18,[](int state){
    menuTimeout = millis()+30000; //reset the menu close time
    if(state && bMenu){ //if the menu is open
      if(level["opts"]){
        level["ind"] =(level["ind"].as<int>()+1)%level["opts"].size(); //if the current level is a submenu, increment and modulo the index to the number of opts
      } else if(level.containsKey("type")){
        if(level["type"] == "kb"){
          if(++kbInd > level["range"][1].as<int>()) kbInd = level["range"][0].as<int>(); //increment the kb character index, and keep it with the kb range.
        } else if(level["type"] == "toggle"){
          toggleInd =!toggleInd; //if it's a toggle, toggle it. 
        } else if(level["name"] == "ssid"){ //increment through the SSID options.
          ssidInd = (ssidInd+1)%numSsids;
          Serial.print(ssidInd);
          Serial.print(" of ");
          Serial.println(numSsids);
        }
      }
      drawMenu();
      spr.pushSprite(0,0); 
    } 
  },20); //20 is the debounce time for the button; any press shorter than 20ms won't be counted.

////////////////////////////////////////////////////////////////////////////
// Dog Rendering callback
////////////////////////////////////////////////////////////////////////
  

  dog.setDrawCB([](){
    getDogFrame(); //get the current dog frame.
    scale(&bgSpr,&spr,0,0,2,false); //scale the background into the frame buffer.

    emojiFrame(POOP); //get teh poop picture.
    for(int i=0; i<curPoops; i++){ //draw all the poops
      scale(&emojiSpr,&spr,poops[i].x,poops[i].y,2,false);
    }
    scale(&dogSpr,&spr,dog.x,dog.y,4,dog.shouldMirror()); //scale the dog into the frame buffer

    emojiFrame(dog.state); //get teh dog emotions
    if(dog.noteTmr > millis() || dog.note){ //draw the bubbles
      scale(&bubbleSpr,&spr,dog.x+26,dog.y-10,2,dog.left);
      scale(&emojiSpr,&spr,dog.x+44,dog.y+0,2,false);
    }

    if(ballChase){ //if we're chasing the ball
      emojiFrame(PLAY); //get the ball picture
      if(ballTmr > millis()){ //get the chase timer
        float pos = (ballTmr -millis())/1000.; //get the percentage of time left in the throw
        scale(&emojiSpr,&spr,-32+(1.-pos)*272,60 - sin(pos*PI)*40,2,false); //put the ball on an arc depending on the pos percentage
      } else if(ballTmr + 10000 > millis()){ //for ten seconds after the throw, run around carrying the ball.
        if(hasBall){
          scale(&emojiSpr,&spr,dog.x+88,dog.y+68,2,false);
        } else if(dog.x>=240) hasBall = true;
        dog.action(RUN,true);
        dog.left = false;
      } else ballChase = 0, dog.free(), hasBall = false;
    }

    if(cfgDoc["clock"].as<bool>() && WiFi.status() == WL_CONNECTED){ //if the wifi connected, and we're displaying the clock
      struct tm timeinfo;
      if(!getLocalTime(&timeinfo)){//get the current time
        Serial.println("Failed to obtain time");
        return;
      }

      //make a 50% opaque black rectangle at the top of the screen
      for(int i=0; i<55; i++){
        for(int j=0; j<spr.width(); j++){
          uint32_t col = spr.readPixel(j,i);
          col = fastBlend(128, col, TFT_BLACK);
          spr.drawPixel(j, i, col);
        }
      }

      //format the time and print it.
      char timeHM[6];
      strftime(timeHM,6, "%H:%M", &timeinfo);
      spr.setTextSize(4);
      int twid = spr.textWidth(timeHM);
      spr.setCursor(120 - twid/2, 20);
      spr.setTextColor(TFT_WHITE);
      spr.print(timeHM);
      spr.setTextSize(2);

      //print the temperature string at the top of the page
      twid = spr.textWidth(tempString);
      spr.setCursor(120 - twid/2, 2);
      spr.setTextColor(TFT_WHITE);
      spr.print(tempString);
      spr.drawArc(120+twid/2+2, 4, 2, 1, 0, 360, TFT_WHITE, TFT_BLACK, true);
    }

    drawMenu();

    //push the frame buffer to the screen.
    spr.pushSprite(0,0);
  });

  Serial.println("\r\nInitialisation done.");

  //let the dog run around.
  dog.free();
}

void drawMenu(){
  if(bMenu){ //if the menu is open
    spr.fillRect(0, 240-36, 240, 36, TFT_BLACK); //draw the black rectangle at the bottom of the page
    JsonObject cur = level["opts"][level["ind"].as<int>()].as<JsonObject>(); //get the selected option
    if(cur.containsKey("icon")){ //if the option has an icon, draw it.
      emojiFrame(cur["icon"].as<int>());
      scale(&emojiSpr,&spr,120-16,240-34,2,false);
    } else if(level.containsKey("type")){ //handle "type" options
      if(level["name"] == "mac") menuText(WiFi.macAddress(),1); //display the mac address
      else if(level["name"] == "ssid"){
        menuText(WiFi.SSID(ssidInd)); //display the SSID at the ssidInd index
      }
      if(level["type"] == "kb"){ //if we're in a kb input, draw the input string and the next char (next char is in blue)
        spr.setTextSize(2);
        int twid = spr.textWidth(kbString)+tft.textWidth(String(kbInd));
        if(twid > 200) spr.setTextSize(1);
        twid = spr.textWidth(kbString)+tft.textWidth(String(kbInd));
        spr.setCursor(120 - twid/2, 240-26);
        spr.setTextColor(TFT_WHITE);
        spr.print(kbString);
        spr.setTextColor(TFT_BLUE);
        spr.print(String(kbInd));
      } else if(level["type"] == "toggle"){
        menuText(toggleChoices[toggleInd]); //display the toggle string (on or off)
      }
    } else { //if there is no icon, and not type, just draw the name.
      menuText(cur["name"].as<String>());
    }
    
    if(menuTimeout < millis()) bMenu = false, menuLevel = 0; //close the menu after timeout expires.
  }
}


//====================================================================================
//                                    Loop
//====================================================================================
void loop()
{
  leftBut.idle(); //watch the left button
  rightBut.idle(); //watch the right button
  midBut.idle(); //watch the middle button
  dog.idle(); //run the dog control functions and draw stuff
  idleTimers(); //check the timers.

  //monitor the press timers, for long press functions. This should be integrated as part of the button class.
  if(watchBackPress && millis()-backPressTimer > 1000) watchBackPress = false, ignoreBackRelease = true, previousLevel();
  if(watchMenuPress && millis()-menuPressTimer > 1000){
    watchMenuPress = false;
    ignoreSelectRelease = true;
    if(level["name"]=="password") cfgDoc["password"][curSSID] = kbString;
    else if(level["name"]=="weather") cfgDoc["zipcode"] = kbString;
    writeConfig();
    previousLevel();
  }
}

void getDogFrame(){
  dogSpr.pushImage(-32*(dog.counter), -32*dog.act, SMALLDOGS_WIDTH, SMALLDOGS_HEIGHT, smallDogs); //push the dog image into the sprite at the correct offset for the frame.
}

void emojiFrame(int num){
  emojiSpr.pushImage(-16*num, 0, EMOJIS_WIDTH, EMOJIS_HEIGHT, emojis); //each icon is 16 pixels wide; -16*num offsets to the numth icon.
}

void scale(TFT_eSprite * src, TFT_eSprite * dest, int x, int y, int fac, bool mirror){
  for(int i=0; i<src->height(); i++){ //for each row of the source sprite
    for(int j=0; j<src->width(); j++){ //for each pixel in the row
      uint32_t col = src->readPixel(j,i); //get the current color
      bool shadow = false;
      if(col == TFT_BLACK){ //if the pixel is true black, set the shadow flag
        shadow = true;
      } 
      if(col != TFT_GREEN){ //if the pixel is not full green (meaning green pixels aren't drawn, so they're transparent, effectively)
        if(!daytime) col = fastBlend(128, col, TFT_NAVY); //blend the pixel color from the source image with navy, if it's after sunset
        for(int h=0; h<fac; h++){ //these for-loops scale each pixel up by a factor of fac
          for(int w=0; w<fac; w++){ //this makes each pixel from src cover fac*fac pixels in dest.
            int xaug = j*fac + w; //j is the x-coordinate of the src pixel
            if(mirror) xaug = src->width()*fac-xaug; //if we are drawing the image mirrored, reverse the coordinate across the source width
            if(shadow){ //if the shadow flag is set, blend the destination color with black
              uint32_t bgcol = dest->readPixel(x+xaug,y+i*fac+h);
              col = fastBlend(128, bgcol, TFT_BLACK);
            }
            dest->drawPixel(x+xaug, y+i*fac+h,col); //draw the scaled pixel.
          }
        }
      }
    }
  }
}

