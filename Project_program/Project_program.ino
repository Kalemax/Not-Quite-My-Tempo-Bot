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
#define BUZZER_PIN 10

//Defines how long each click of the metronome is in ms
#define CLICK_LENGTH 30

//Defines the size of arrays (any piece with this amount of beats or fewer is valid)
#define MAX_ITEMS 1600

//Piezo disc stuff for inputting sound
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
float totalBeats; //Holds total amount of beats based on the measure selection

//Time measurements
unsigned long lastHitTime = 0; //Used to hold the time in ms of the last hit
unsigned long startTime; //Holds the time when recording starts after measure/tempo selection
unsigned long currentTime;
unsigned long hitTimes[MAX_ITEMS]; //A hit is when the piezo picks up a note, so this array stores the time in ms everytime that happens since the start of recording

//Used to calculate how much the user was rushing or dragging
float absSum;
float signedSum;
float avgAbs;
float avgSigned;
float errorMargin; //How off the average note has to be for it to be considered not on tempo
float biasMargin; //How lopsided (between early and late) the average note has to be for it to be considered rushing/dragging


String feedback; //Will hold the feedback message that is displayed after recording
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

  //Makes piezo disc more sensitive

  analogSetPinAttenuation(PIEZO_PIN, ADC_0db);
  
  //Sets up the buzzer output for the metronome ticking
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH);

  //Displays an error message in the console if the display is not found
  if (!display.begin(OLED_ADDR, true)) {
    Serial.println("OLED not found");
    while (true) delay(10);
  }
  LittleFS.begin(true); //Initializes the internal storage in the ESP32
  
  //Loads the data from the piece into the program
  loadPiece(); //Note that if you want to change the piece, you have to run the LittleFS Upload again with your new piece.txt file
  Serial.println(maxMeasures); //test

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
      //When this goes through, the recording will start, so all values should be reset
      hitCount = 0;
      absSum = 0;
      signedSum = 0;
      lastHit = false;
      msPerBeat = 60000.0 / bpm; //Holds how many milliseconds are in a beat at the selected bpm
      totalBeats = (measures < maxMeasures) ? measureStarts[measures] : measureStarts[maxMeasures - 1] + beatsPerMeasure; //Calculates total number of beats
      measureSelect = false;
      //Count off before recording starts
      for(int i = 0; i < beatsPerMeasure; i++){
        digitalWrite(BUZZER_PIN, LOW); //Low signal starts beep
        delay(CLICK_LENGTH);
        digitalWrite(BUZZER_PIN, HIGH); //Stops beep very quickly after to sound like a click
        delay(msPerBeat);
      }
      displayRecording();
      recording = true;
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
    hit = (piezoReading > THRESHOLD); //Counts a hit if the piezo disc picks up a high enough signal
    currentTime = millis() - startTime; //Starts the time from right after the count off
    if(hit && !lastHit && millis() - lastHitTime > TIME_GAP && hitCount < MAX_ITEMS){
      lastHitTime = millis();
      //Adds a new time to hit times
      hitTimes[hitCount++] = currentTime;
      //Displays hit count and times for testing purposes
      Serial.print(hitCount);
      Serial.print(" ");
      Serial.println(currentTime);
    }
    lastHit = hit;

    //Stops recording after the time of the selected measures has passed
    if(currentTime >= totalBeats * msPerBeat){
      recording = false;

      //Beeps when recording is done
      digitalWrite(BUZZER_PIN, LOW);
      delay(400);
      digitalWrite(BUZZER_PIN, HIGH);

      //Loops through list and finds difference between expected and actual ms value for each hit
      float difference;
      for (int i = 0; i < hitCount && i < noteCount; i++) {
        difference = hitTimes[i] - (noteOffsets[i] * msPerBeat); //Actual minus expected; positive difference means late, negative means early
        //Adds the differences to the signed/unsigned sums
        absSum += fabs(difference);
        signedSum += difference;
        
        //Prints results
        Serial.print("Expected: ");
        Serial.println(noteOffsets[i] * msPerBeat);
        Serial.print("Difference: ");
        Serial.println(difference);
      }

      //Takes average of how off user is
      float avgAbs = absSum / hitCount;
      float avgSigned = signedSum / hitCount;

      errorMargin = 0.10 * msPerBeat;
      biasMargin = 0.05 * msPerBeat;

      if(hitCount == 0){
        feedback = "No notes  detected"; //The extra space is there on purpose so it will fit on the screen
      }else if (avgAbs <= errorMargin) {
        feedback = "Good!"; //Relatively on tempo
      }else if (avgSigned > biasMargin) {
        feedback = "Dragging!"; //Playing too late
      }else if (avgSigned < -biasMargin) {
        feedback = "Rushing!"; //Playing too early
      }else {
        feedback = "NOT MY TEMPO!"; //Tempo is all over the place
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
  //Reads the numbers on line 1 of piece.txt (how many quarter note beats are in a typical measure)
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

//Determines what feedback to give then displays it on the screen
void displayFeedback(){
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.print(feedback); //Displays feedback message
  display.display();
}


