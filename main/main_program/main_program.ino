//10Sep

//important
//show events (what from when to when)
//show temperature and weather (add to printEvents() function)
//get alarm time every midnight
//get cal data from firebase at midnight or event end  (use ||)

//optimise the JS webapp for it
//get all data from firebase and print it prettily on LCD (everything)
//control backlight and LED on LEDSW
//battery system
//finalise outer casing


//todo:
//cloud function + calendar https://developers.google.com/calendar/quickstart/js
//setup google auth so clocky can mass produce, not just for ur firebase acc
//optimize firebase / zapier to efficiency when get data
//refactor, dun use so much char day[] things 


//done:
//change alarm to LED see how, 
//buzzer sound at right time, use LEDSWITCH to stop alarm
//test SwitchLED, when alarm sound only light up
//get alarm time from firebase on device boot
//scrolling text
//complete PCB, program from PCB
//interface for changing alarm time, 
//parse the cacat google calendar time (done with substrings)
//get weather from accuweather https://github.com/ImJustChew/cec-iclock/blob/master/functions/index.js
//get time from firebase to readjust itself every 45 mins (need test) https://github.com/scanlime/esp8266-Arduino/blob/master/tests/Time/Time.ino

// discontinued ideas
//use buttons to adjust alarm
//save alarm time in EEPROM

// RTC 3V/5V , LCD 5V
// Buzzer 5V
// Both switches Input -> [Switch] -> Ground, NodeMCU internal Pullup
//the switches are inverted (INPUT_PULLUP)
//the outputs / LEDs are not


// ESP8266 Pinouts
// D0   = 16; 
// D1   = 5; SCL of RTC and LCD
// D2   = 4; SDA of ETC and LCD
// D3   = 0;
// D4   = 2;
// D5   = 14; Buzzer +ve Pin
// D6   = 12; 
// D7   = 13; StopSwitch (LED)
// D8   = 15; StopSwitch (sw)
// D9   = 3;
// D10  = 1;


// CLOCKYPCB NodeMCU Pinouts
// D0   = 16; 
// D1   = 5; SCL of RTC and LCD
// D2   = 4; SDA of ETC and LCD
// D3   = 0;
// D4   = 2; Buzzer +ve Pin
// D5   = 14; StopSwitch (sw)
// D6   = 12; StopSwitch (LED)
// D7   = 13; 
// D8   = 15; 
// D9   = 3;
// D10  = 1;

#include <Wire.h>
#include "RTClib.h"
#include "FirebaseESP8266.h"
#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <EEPROM.h>

#define FIREBASE_HOST "esp8266-f2775.firebaseio.com"
#define FIREBASE_AUTH "EQ0xXkArCNnNnDlrd7kxaroYoTULU1lZgDPLtD9L"
#define WIFI_SSID "ZONGZ"
#define WIFI_PASSWORD "zz12343705"

//pcb pins
#define alarm 2 //D4
#define StopSW 14 //D5
#define StopSWLED 12 //D6  

FirebaseData firebaseData;
LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS3231 rtc;

String summary = "";
String begins = "";
String beginsDate = "";
String beginsTime = "";

String ends = "";
String endsDate = "";
String endsTime  = "";

String alarmTime = "";

bool alarmState = 0;

byte leftPattern[] = {
  B01010,
  B10101,
  B01010,
  B10101,
  B01010,
  B10101,
  B01010,
  B10101
};

byte rightPattern[] = {
  B10101,
  B01010,
  B10101,
  B01010,
  B10101,
  B01010,
  B10101,
  B01010
};

byte Arm[] = {
  B00001,
  B00001,
  B00001,
  B00010,
  B00010,
  B00100,
  B01000,
  B00000
};

byte Eye[] = {
  B00000,
  B01110,
  B10011,
  B10111,
  B11111,
  B01110,
  B00000,
  B00000
};

byte Mouth[] = {
  B00000,
  B00000,
  B01110,
  B01010,
  B00010,
  B00010,
  B01100,
  B00000
};

byte Star[] = {
  B00000,
  B00100,
  B00100,
  B01010,
  B10101,
  B01010,
  B00100,
  B00100
};


//alarm time
//DateTime actualTime;
int shiftedIndexes = 0;
String message = "";

DateTime before45Mins;

void setup (){
  Serial.begin(115200);

  lcd.begin(16, 2);  
  lcd.init();

  lcd.createChar(0, leftPattern);
  lcd.createChar(1, rightPattern);
  lcd.createChar(2, Arm);
  lcd.createChar(3, Eye);
  lcd.createChar(4, Mouth);
  lcd.createChar(5, Star);
  
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("connecting......");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);

  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }
  
//  if (rtc.lostPower()) {
//    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
//  }

  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  
  pinMode(alarm, OUTPUT);      //buzzer
  pinMode(StopSW, INPUT_PULLUP); //StopSW
  pinMode(StopSWLED, OUTPUT); //StopSWLED

  //get some message on startup
  getAlarmTime();
  updateEventsFromFirebase();

  before45Mins = rtc.now();
  
  lcd.clear();
}

void loop (){
  DateTime now = rtc.now();
  
  showDate(now);

  char rightNow[] = "hh:mm:ss";
  char dateRightNow[] = "YYYY-MM-DD";
  String stringyTime = String(now.toString(rightNow));
  String stringyDate = String(now.toString(dateRightNow));
  Serial.print("the alarm i got: ");
  Serial.println(alarmTime);
  Serial.print("the time rn: ");
  Serial.println(stringyTime);
  Serial.print("the date rn: ");
  Serial.println(stringyDate);

  if((stringyTime == "00:00:00") || ((stringyDate == endsDate) && (stringyTime == endsTime))){
    updateEventsFromFirebase();
    getAlarmTime();
  }
  
  if((stringyTime == alarmTime) && (alarmState == 0)){
    alarmState = 1;
    Serial.println("WAKEY WAKEY MDF WAKEY WAKEY MDF WAKEY WAKEY MDF WAKEY WAKEY MDF ");
  }
  
  if((alarmState == 1) && (digitalRead(StopSW) == LOW)){
    alarmState = 0;
    Serial.println("!!!!!!!!!!!!!!!!!!!! Alarm Stopped !!!!!!!!!!!!!!!!!!!!");
  }
  
  digitalWrite(alarm, alarmState);
  digitalWrite(StopSWLED, alarmState);

  printEvents(message, shiftedIndexes);
  //scrolling mechanism
  if(shiftedIndexes > message.length()){
    shiftedIndexes = 0;
    printKaomoji();
    
  } else{
    shiftedIndexes++;
  }  

  Serial.print("end date: ");
  Serial.println(endsDate);
  Serial.print("end time: ");
  Serial.println(endsTime);
  Serial.println("-----------------------------------------------------------------------------------");

  adjustTimeFromFirebase(now);
  delay(1000);
  lcd.clear();
}

void adjustTimeFromFirebase(DateTime theTime){

  String dateRN = "";
  String timeRN = "";
  char dateFormat[] = "YYYY-MM-DD";
  char timeFormat[] = "hh:mm:ss";

  if((theTime - TimeSpan(0,45,0,0)) == before45Mins){
    Serial.println("ok im starting callibreting!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    if (Firebase.getString(firebaseData, "/calendar/accuracy/dateNow")){
      dateRN = firebaseData.stringData();
    } else {
      Serial.print("Error in getString: ");
      Serial.println(firebaseData.errorReason());
    } 
    if (Firebase.getString(firebaseData, "/calendar/accuracy/timeNow")){
      timeRN = firebaseData.stringData();
    } else {
      Serial.print("Error in getString: ");
      Serial.println(firebaseData.errorReason());
    }

    if(
      (String(theTime.toString(timeFormat)) != timeRN) || 
      (String(theTime.toString(dateFormat)) != dateRN)
    ){
      rtc.adjust(DateTime(
        (dateRN.substring(0,4)).toInt(),
        (dateRN.substring(5,7)).toInt(),
        (dateRN.substring(8,11)).toInt(),
        (timeRN.substring(0,2)).toInt(),
        (timeRN.substring(3,5)).toInt(),
        (timeRN.substring(6,8)).toInt()
      ));
    }
    
    before45Mins = theTime;
  }
  
}

void printEvents(String message, int shiftedIndexes){
  String toPrint = "";
  toPrint += message.substring(shiftedIndexes, (shiftedIndexes+16));
//  toPrint += " " + String(rtc.getTemperature()).substring(0,2) + (char)223 + "C";  
//add firebase weather, event start and end

  lcd.setCursor(0, 1);
  lcd.print(toPrint);

}

void showDate(DateTime dt){
  char Date[] = "DDMMM";
  lcd.setCursor(0, 0);
  lcd.print(dt.toString(Date));
  
  lcd.write(byte(0));
  lcd.write(1);
  
  char TimeRN[] = "hh:mm:ss";
  lcd.print(dt.toString(TimeRN));
}

void getAlarmTime(){
  if (Firebase.getString(firebaseData, "/calendar/alarmTime")){
    alarmTime = firebaseData.stringData();
  } else {
    Serial.print("Error in getString: ");
    Serial.println(firebaseData.errorReason());
  }
}

void updateEventsFromFirebase(){
  if (Firebase.getString(firebaseData, "/calendar/event/summary")){
    message = firebaseData.stringData();
  } else {
    Serial.print("Error in getString: ");
    Serial.println(firebaseData.errorReason());
  }
  if (Firebase.getString(firebaseData, "/calendar/event/begins")){
    begins = firebaseData.stringData();
    beginsDate = begins.substring(0, 10);
    beginsTime = begins.substring(11, 19);
  } else {
    Serial.print("Error in getString: ");
    Serial.println(firebaseData.errorReason());
  }
  if (Firebase.getString(firebaseData, "/calendar/event/ends")){
    ends = firebaseData.stringData();
    endsDate = ends.substring(0, 10);
    endsTime = ends.substring(11, 19);
  } else {
    Serial.print("Error in getString: ");
    Serial.println(firebaseData.errorReason());
  }
}

void printKaomoji(){
  //(ﾉ◕ヮ◕)ﾉ✧
  lcd.setCursor(0,1);
  lcd.print("(");
  lcd.write(2); //arm
  lcd.write(3); //eye
  lcd.write(4); //mouth
  lcd.write(3); //eye
  lcd.print(")");
  lcd.write(2); //arm
  lcd.write(5); //star
}

//----------------------------------------------- RANDOM CODES I DIDNT WANT TO DELETE -----------------------------------------------
//  lcd.autoscroll();
//  // print from 0 to 9:
//  for (int thisChar = 0; thisChar < 10; thisChar++) {
//    lcd.print(thisChar);
//    delay(500);
//  }
//  // turn off automatic scrolling
//  lcd.noAutoscroll();

//void checkEvents(DateTime dt){
//  char currently[] = "hh:mm";
//  Serial.println(dt.toString(currently));
//
//  //if event ended OR is currently midnight, update summary and start and end time
//  
//  if(dt.toString(currently) == "00:00"){
//    if (Firebase.getString(firebaseData, "/calendar/event/summary")){
//      summary = firebaseData.stringData();
//    } else {
//      Serial.print("Error in getString: ");
//      Serial.println(firebaseData.errorReason());
//    }
//    if (Firebase.getString(firebaseData, "/calendar/event/begins")){
//      begins = firebaseData.stringData();
//    } else {
//      Serial.print("Error in getString: ");
//      Serial.println(firebaseData.errorReason());
//    }
//    if (Firebase.getString(firebaseData, "/calendar/event/ends")){
//      ends = firebaseData.stringData();
//    } else {
//      Serial.print("Error in getString: ");
//      Serial.println(firebaseData.errorReason());
//    }
//  }
//}

//void printEvents(DateTime dt){
//  char dayName[] = "DDD";
//  String today = dt.toString(dayName);
//  if(summary == ""){
//    Serial.println(today + ": Nothing Scheduled!");
//  }
//  else{
//    Serial.println(today + ": " + summary + " from " + begins + " to " + ends);
//  }
//}

//  String timetest = theTime.toString(timeFormat);
//  String datetest = theTime.toString(dateFormat);
//  Serial.print("year: ");
//  Serial.println(datetest.substring(0,4));
//  Serial.print("month: ");
//  Serial.println(datetest.substring(5,7));
//  Serial.print("day: ");
//  Serial.println(datetest.substring(8,11));
//  Serial.print("hour: ");
//  Serial.println(timetest.substring(0,2));
//  Serial.print("minute: ");
//  Serial.println(timetest.substring(3,5));
//  Serial.print("second: ");
//  Serial.println(timetest.substring(6,8));

/////////////////////////////////////////////////// references to the bottom ///////////////////////////////////////////////////

//The toString() buffer can be defined using following combinations:
  //hh - the hour with a leading zero (00 to 23)
  //mm - the minute with a leading zero (00 to 59)
  //ss - the whole second with a leading zero where applicable (00 to 59)
  //YYYY - the year as four digit number
  //YY - the year as two digit number (00-99)
  //MM - the month as number with a leading zero (01-12)
  //MMM - the abbreviated English month name ('Jan' to 'Dec')
  //DD - the day as number with a leading zero (01 to 31)
  //DDD - the abbreviated English day name ('Mon' to 'Sun')

  void types(String a) { Serial.println("it's a String"); }
  void types(int a) { Serial.println("it's an int"); }
  void types(char *a) { Serial.println("it's a char*"); }
  void types(float a) { Serial.println("it's a float"); }
  void types(bool a) { Serial.println("it's a bool"); }

//DateTime is an OBJECT
//dt.hour() and stuff are ints
//rtc.getTemperature() is a float
//the toString()s are char*s

//toggle input
//  if(digitalRead(sw1)== LOW){ alarmState = !alarmState; Serial.println("Switch is toggled")}

//push input
//  if(digitalRead(sw1) == LOW){ 
//    alarmState = 1; 
//    Serial.println("Switch is momentarily pressed"); 
//  } else {
//    alarmState = 0;
//  }
