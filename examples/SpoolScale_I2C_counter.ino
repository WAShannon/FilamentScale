#include <Wire.h>  // Comes with Arduino IDE
#include "HX711.h" //https://github.com/bogde/HX711
//#include "LiquidCrystal.h"
#include <LiquidCrystal_I2C.h>

// Configuration
int inPin = 7; // Button pin tare
int zeroPin = 8 ;//Button pin zero

float Onemm_weight = 3.03; // Average weight per 1.75 mm filament meter
float Threemm_weight = 2.5; // Average weight per 3mm filament meter
float spoolWeight = 200; // Average weight of spool

// initialize the library with the numbers of the interface pins
//LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
//int backLight = 9;    // LCD pin 13 will control the backlight
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  // Set the LCD I2C address

// HX711.DOUT	- pin #A2
// HX711.PD_SCK	- pin #A3

HX711 scale(A2, A3);		// parameter "gain" is ommited; the default value 128 is used by the library
int val = 0; // variable for reading the pin
int val2 = 0;
int incoming = 0;
float weight = 0.0;
//int showWeight = 0;
float showWeight = 0.00;
//int fMeters = 0;
float fMeters = 0.00;
float weight_div = Onemm_weight;
float startWeight = 0.00;
float usedWeight = 0.00;
float usedMeters = 0.00;

void setup() {
  Serial.begin(38400);
//  Serial.begin(115200);  // Used to type in characters
  lcd.begin(20,4);         // initialize the lcd for 20 chars 4 lines and turn on backlight
  
  pinMode(inPin, INPUT); // declare pushbutton
  pinMode(zeroPin, INPUT); //declare pushbutton
 
  digitalWrite(inPin, HIGH);  // turn on pullup resistors
  digitalWrite(zeroPin, HIGH);  // turn on pullup resistors
  
  //pinMode(backLight, OUTPUT);
  //digitalWrite(backLight, HIGH); // turn backlight on. Replace 'HIGH' with 'LOW' to turn it off.
  Serial.println("Spool Scale");
  // set up the LCD's number of columns and rows: 
  //lcd.begin(20,4);
  
  // Print a message to the LCD.
  lcd.setCursor(4,0);
  lcd.print("Spool Scale");
  lcd.setCursor(9,1);
  lcd.print("By");
  lcd.setCursor(4,2);
  lcd.print("Robert Haddad");
  lcd.setCursor(0,3);
  lcd.print("Modified by Bin Sun");
  delay(3000);
  
  lcd.clear();
  scale.set_scale(422); // Set this to empty scale.set_scale() for calibration
  scale.tare();
  checkFil();
}  

void checkFil(){
  lcd.setCursor(0,0);
  lcd.print("Are you using ABS");
  lcd.setCursor(0,1);
  lcd.print("Filament? Hold Tare");
  lcd.setCursor(0,2);
  lcd.print("for YES");
  lcd.setCursor(0,3);
  boolean tarePressed = false;
  int countdown = 3;
  while (!tarePressed && countdown >= 0){
    val = digitalRead(inPin);
    if(val == LOW){
      lcd.clear();
      lcd.setCursor(0,2);
      lcd.print("Filament set to ABS");
      delay(1000);
      tarePressed = true;
      weight_div = Threemm_weight;
      lcd.clear();
    }
    lcd.setCursor(0,3);
    lcd.print("Countdown:   ");
    lcd.setCursor(0,3);
    lcd.print("Countdown: ");
    lcd.print(countdown);
    countdown--;
    if(countdown <0){
      lcd.clear();
      lcd.setCursor(0,0);
    }
    if(tarePressed){
      lcd.clear();
      lcd.setCursor(0,0);
    }
    delay(1000);
  }
}
    
void loop() {
  val = digitalRead(inPin); // read input value
  val2 = digitalRead(zeroPin); //read input value
  
  if(val == LOW) {
    lcd.clear();
    lcd.setCursor(7,2);
    lcd.print("Taring");
    delay(500);
    scale.tare();
    startWeight = 0.00;
    lcd.clear();
  }

  lcd.setCursor(0,0);
  weight = scale.get_units(20);
  Serial.println(weight);
  showWeight = weight;
 // startWeight = weight;
  lcd.print("                    ");
  lcd.setCursor(0,0);
  lcd.print("Weight~ ");
  lcd.print(showWeight);
  lcd.print(" g");
  lcd.setCursor(0,1);
  lcd.print("                    ");
  lcd.setCursor(0,1);
  lcd.print("Fil. Left~ ");
  fMeters = ((showWeight-spoolWeight)/weight_div);
  lcd.print(fMeters);
  lcd.print(" m");

  if(val2 == LOW) {
    startWeight = showWeight;
  }

  lcd.setCursor(0,2);
  lcd.print("                    ");
  lcd.setCursor(0,2);
  lcd.print("Used Wt.~ ");
  usedWeight = startWeight-showWeight;
  lcd.print(usedWeight);
  lcd.print(" g");
//  lcd.setCursor(0,3);
//  lcd.print("                    ");
//  lcd.setCursor(0,3);
//  lcd.print("Fil. Used~ ");
//  usedMeters = usedWeight/weight_div;
//  lcd.print(usedMeters);
//  lcd.print(" m");
  
  if(fMeters <= 2){
    lcd.setCursor(0,3);
    lcd.print("                    ");
    lcd.setCursor(1,3);
    lcd.print("* Change Filament!");
  } else {
  lcd.setCursor(0,3);
  lcd.print("                    ");
  lcd.setCursor(0,3);
  lcd.print("Fil. Used~ ");
  usedMeters = usedWeight/weight_div;
  lcd.print(usedMeters);
  lcd.print(" m");
  }
}
