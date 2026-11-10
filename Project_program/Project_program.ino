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

//Define pins
#define UP_PIN 22
#define CENTER_PIN 21
#define DOWN_PIN 20
#define PIEZO_PIN 1

//Defines the size of arrays (any piece with this amount of beats or fewer is valid)
#define MAX_ITEMS 1600

//Piezo stuff
#define THRESHOLD 100 //Threshold voltage that the piezo should read as a hit
#define TIME_GAP 40 //If another hit is read after a hit within this many milliseconds, it does not count
bool hit = false;
bool lastHit = false;
int hitCount = 0;


int bpm = 100; //Tempo that you will play at
int measures = 1; //Number of measures that you will play
bool lastUp = HIGH; //Starts high
bool lastDown = HIGH;
bool lastCenter = HIGH;

//Determines what screen to be on
bool tempoSelect = true;
bool measureSelect = false;
bool recording = false;

//Sets limits
int minBpm = 50;
int minMeasures = 1;
int maxBpm = 300;
int maxMeasures; //Pull number of measures from python script

//Data from sheet music
float beatsPerMeasure;
float measureStarts[MAX_ITEMS];
float noteOffsets[MAX_ITEMS];
int noteCount = 0;
//Declared after tempo and measure selection
float msPerBeat; //Holds how many ms pass in each beat
float countOff; //Holds the amount of time to wait for the countoff before recording
float totalBeats; //Holds total amount of beats based on the measure selection

//Time measurements
unsigned long lastHitTime = 0; //Used to hold the time in ms of the last hit
unsigned long startTime; //Holds the time when recording starts after measure/tempo selection
unsigned long currentTime;
unsigned long hitTimes[MAX_ITEMS]; //A hit is when the piezo picks up a note, so this array stores the time in ms everytime that happens since the start of recording


//Allows the list to be converted into floats rather than a long string of numbers
int parseFloats(String line, float arr[], int maxLen) {
  int count = 0;
  int start = 0;
  while (start < line.length() && count < maxLen) {
    int comma = line.indexOf(',', start);
    if (comma == -1) comma = line.length();
    arr[count++] = line.substring(start, comma).toFloat();
    start = comma + 1;
  }
  return count;
}

Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

//Runs once on startup
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
    Serial.println("OLED not found");
    while (true) delay(10);
  }
  LittleFS.begin(true); //Initializes the internal storage in the ESP32
  
  //Loads the data from the piece into the program
  loadPiece();

  //Starts by allowing the user to choose the tempo they want to play at
  displayBpm();
}

void loop() {

  //Reads when the buttons are pressed (voltage becomes low, high voltage is default)
  bool up = digitalRead(UP_PIN); //Push up to increase value
  bool down = digitalRead(DOWN_PIN); //Push down to decrease value
  bool center = digitalRead(CENTER_PIN); //Push center to continue to next screen
  /*Note that in practice, 3 buttons will not be used, but rather one button with 3 different outputs*/

  if(center == LOW && lastCenter == HIGH){
    if(tempoSelect){
      displayMeasures();
      measureSelect = true;
      tempoSelect = false;
    }else if(measureSelect){
      //When this goes through, the recording will start 
      hitCount = 0;
      lastHit = false;
      msPerBeat = 60000.0 / bpm; //Holds how many milliseconds are in a beat at the selected bpm
      countOff = msPerBeat * beatsPerMeasure; //Calculates how many ms the count off is
      totalBeats = (measures < maxMeasures) ? measureStarts[measures] : measureStarts[maxMeasures - 1] + beatsPerMeasure; //Calculates total number of beats
      displayRecording();
      delay(countOff); //Gives time for the countoff before hits are detected
      recording = true;
      measureSelect = false;
      startTime = millis();
    }
  }

  if(up == LOW && lastUp == HIGH){
    if(tempoSelect && bpm < maxBpm){
      bpm += 10;
      displayBpm();
    }else if(measureSelect && measures < maxMeasures){
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

  if(recording){
    int piezoReading = analogRead(PIEZO_PIN);
    hit = (piezoReading > THRESHOLD);
    currentTime = millis() - startTime;
    if(hit && !lastHit && millis() - lastHitTime > TIME_GAP && hitCount < MAX_ITEMS){
      lastHitTime = millis();
      hitTimes[hitCount++] = currentTime;
      //Displays hit count and times for testing purposes
      Serial.print(hitCount);
      Serial.print(" ");
      Serial.println(currentTime);
    }
    lastHit = hit;
    if(currentTime >= totalBeats * msPerBeat){
      recording = false;

      //Loops through list and finds difference between expected and actual ms value for each hit (clean up later)
      float difference;
      for (int i = 0; i < hitCount && i < noteCount; i++) {
        difference = hitTimes[i] - (noteOffsets[i] * msPerBeat);
        Serial.print(difference);
        Serial.print(", ");
      }
      Serial.println();
      displayFeedback();
    }
    
  }

  lastUp = up; //Makes sure that button only triggers once when button is pressed
  lastDown = down;
  lastCenter = center;
}


void loadPiece(){
  File pieceFile = LittleFS.open("/piece.txt", "r"); //piece.txt must be stored in a "data" folder, and this program can only read it
  if(!pieceFile){
    displayError();
    while (true) delay(10); //Stops program from running if no piece is found
  }
  //Reads the numbers on line 1 of piece.txt
  beatsPerMeasure = pieceFile.readStringUntil('\n').toFloat();

  //Reads the lists on lines 2 and 3 and turns them into arrays of floats
  String secondLine = pieceFile.readStringUntil('\n');
  String thirdLine = pieceFile.readStringUntil('\n');
  secondLine.trim();
  thirdLine.trim();
  maxMeasures = parseFloats(secondLine, measureStarts, MAX_ITEMS);
  noteCount = parseFloats(thirdLine, noteOffsets, MAX_ITEMS);

  pieceFile.close();

  if (noteCount >= MAX_ITEMS) {
    Serial.println("Warning: piece truncated");
  }
}

void displayError(){
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.print("No piece");
  display.display();
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

void displayRecording(){
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.print("Recording");
  display.display();
}

void displayFeedback(){
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.print("Done!"); //Make this display actual feedback later
  display.display();
}


