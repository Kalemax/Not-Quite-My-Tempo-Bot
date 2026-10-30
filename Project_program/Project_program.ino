//Arduino libraries
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

//Libraries for pulling the piece info from the text file
#include <FS.h>
#include <LittleFS.h>

//Display parameters and pins
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C
#define SDA_PIN 6
#define SCL_PIN 7

//Define pins for directions of 5 way navigator
#define UP_PIN 22
#define CENTER_PIN 21
#define DOWN_PIN 20



int bpm = 100; //Tempo that you will play at
int measures = 1; //Number of measures that you will play
bool lastUp = HIGH; //Starts high
bool lastDown = HIGH;
bool lastCenter = HIGH;

//Determines what you are selecting
bool tempoSelect = true;
bool measureSelect = false;

//Sets limits
int minBpm = 50;
int minMeasures = 1;
int maxBpm = 300;
/*int maxMeasures = value from python script*/

Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void setup() {
  Serial.begin(115200);
  //Sets up display
  Wire.begin(SDA_PIN, SCL_PIN);

  //Allows the buttons to work and have high voltage when unpressed and low voltage when pressed
  pinMode(UP_PIN, INPUT_PULLUP); 
  pinMode(DOWN_PIN, INPUT_PULLUP);
  pinMode(CENTER_PIN, INPUT_PULLUP);

  //Displays an error message in the console if the display is not found
  if (!display.begin(OLED_ADDR, true)) {
    Serial.println("OLED not found - check wiring");
    while (true) delay(10);
  }
  LittleFS.begin(true); //Initializes the internal storage in the ESP32
  File pieceFile = LittleFS.open("/piece.txt", "r"); //piece.txt must be stored in a "data" folder, and this program can only read it
  
  //Starts by allowing the user to choose the tempo they want to play at
  displayBpm();
}

void loop() {

  //Reads when the buttons are pressed (voltage becomes low, high voltage is default)
  bool up = digitalRead(UP_PIN); //Push up to increase value
  bool down = digitalRead(DOWN_PIN); //Push down to decrease value
  bool center = digitalRead(CENTER_PIN); //Push center to continue to next screen
  /*Note that in practice, 3 buttons will not be used, but rather one button with 3 different outputs*/

  if(center == LOW && lastCenter == HIGH && tempoSelect){
    displayMeasures();
    measureSelect = true;
    tempoSelect = false;

  }

  if(up == LOW && lastUp == HIGH){
    if(tempoSelect && bpm < maxBpm){
      bpm += 10;
      displayBpm();
    }else if(measureSelect){
      measures++;
      displayMeasures();
    }
  }
  
  if(down == LOW && lastDown == HIGH){
    if(tempoSelect && bpm > minBpm){
      bpm -= 10;
      displayBpm();
    }else if(measureSelect && measures > minMeasures){
      measures--;
      displayMeasures();
    }
    
    
  }

  delay(10);
  lastUp = up; //Makes sure that button only triggers once when button is pressed
  lastDown = down;
  lastCenter = center;
}

void displayBpm() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.print("BPM:");
  display.println(bpm);
  display.display();
}

void displayMeasures(){
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.print("Number of measures: ");
  display.println(measures);
  display.display();
}