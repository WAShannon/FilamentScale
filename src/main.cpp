#include "HX711.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <OneButton.h>

// Pin definitions for the load cell (ESP32)
const int LOADCELL_DOUT_PIN = 32;
const int LOADCELL_SCK_PIN = 33;

// Pin definitions for the rotary encoder (ESP32)
const int ENCODER_CLK_PIN = 16;
const int ENCODER_DT_PIN = 17;
const int ENCODER_SW_PIN = 19;

// Pin definitions for the ST7735 (ESP32)
#define TFT_CS     5
#define TFT_RST    15
#define TFT_DC     2
#define TFT_MOSI   23
#define TFT_SCK    18

// Create an instance of the HX711 class
HX711 scale;

// Create an instance of the ST7735 class
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// Create an instance of the OneButton class
OneButton encoderButton(ENCODER_SW_PIN, true);

// Initial calibration factor (this needs to be adjusted for your specific setup)
float calibration_factor = 2280.0;
float new_calibration_factor = calibration_factor;  // New calibration factor to be adjusted

// Filament details (adjust these values as per your filament specifications)
float empty_spool_weight = 200.0; // in grams (default value)
const float filament_density = 1.24;    // in g/cm^3 for PLA (adjust for your material)
const float filament_diameter = 1.75;   // in mm

// Variables for encoder handling
int lastEncoderCLKState = LOW; // Last state of the CLK pin
int encoderPos = 0;  // Current position of the encoder

// Timing variables
unsigned long previousMillis = 0;
const long interval = 250;  // Interval for updating the display

// Define states for the state machine
enum State {
  CALIBRATION_MODE,
  SPOOL_MODE
};

// Initial state of the state machine (default to SPOOL_MODE)
State currentState = SPOOL_MODE;

// Function prototypes
void setup();
void loop();
void adjustCalibrationFactor();
void adjustSpoolWeight();
void updateDisplay();
void switchMode();
void applyNewCalibrationFactor();
void readEncoder();

void setup() {
  Serial.begin(115200); // ESP32 uses higher baud rates

  // Initialize load cell
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(calibration_factor);  // Set the calibration factor
  scale.tare();                         // Reset the scale to 0

  // Initialize rotary encoder pins
  pinMode(ENCODER_CLK_PIN, INPUT_PULLUP);
  pinMode(ENCODER_DT_PIN, INPUT_PULLUP);
  pinMode(ENCODER_SW_PIN, INPUT_PULLUP);

  // Initialize the display
  tft.initR(INITR_BLACKTAB);  // Initialize a ST7735S chip, black tab
  tft.setRotation(3);         // Rotate the screen by 270 degrees
  tft.fillScreen(ST7735_BLACK);
  tft.setTextColor(ST7735_WHITE);
  tft.setTextSize(1);
  tft.setCursor(0, 0);
  tft.println("Initializing...");

  // Initialize the encoder button
  encoderButton.attachClick(applyNewCalibrationFactor);
  encoderButton.attachLongPressStart(switchMode);

  Serial.println("Load cell and encoder initialized. Place the spool on the load cell.");
}

void loop() {
  // Call the OneButton tick function to detect button events
  encoderButton.tick();

  // Read the encoder
  readEncoder();

  // Perform actions based on the current mode
  switch (currentState) {
    case CALIBRATION_MODE:
      // Handle calibration mode
      adjustCalibrationFactor();
      break;
    case SPOOL_MODE:
      // Handle spool mode
      adjustSpoolWeight();
      break;
  }

  // Update display
  if (millis() - previousMillis >= interval) {
    updateDisplay();
    previousMillis = millis();
  }
}

void readEncoder() {
  int currentEncoderCLKState = digitalRead(ENCODER_CLK_PIN);
  if (currentEncoderCLKState != lastEncoderCLKState && currentEncoderCLKState == LOW) {
    // If the CLK signal has changed and is now LOW
    if (digitalRead(ENCODER_DT_PIN) == HIGH) {
      encoderPos++;
    } else {
      encoderPos--;
    }
    Serial.print("Encoder Position: ");
    Serial.println(encoderPos);
  }
  lastEncoderCLKState = currentEncoderCLKState;
}

void adjustCalibrationFactor() {
  // Adjust calibration factor using encoder
  new_calibration_factor += encoderPos * 10; // Example adjustment
  encoderPos = 0; // Reset encoder position after adjustment
  Serial.print("Adjusted Calibration Factor: ");
  Serial.println(new_calibration_factor);
}

void adjustSpoolWeight() {
  // Adjust empty spool weight using encoder
  empty_spool_weight += encoderPos; // Example adjustment
  encoderPos = 0; // Reset encoder position after adjustment
  Serial.print("Adjusted Spool Weight: ");
  Serial.println(empty_spool_weight);
}

void updateDisplay() {
  tft.fillScreen(ST7735_BLACK); // Clear the screen

  // Update display based on the current mode
  switch (currentState) {
    case CALIBRATION_MODE: {
      // Calculate total weight including the spool
      float total_weight = scale.get_units();

      // Display information
      tft.setCursor(0, 0);
      tft.println("Calibration Mode:");
      tft.print("Current Factor: ");
      tft.println(calibration_factor);
      tft.print("New Factor: ");
      tft.println(new_calibration_factor);
      tft.print("Total Weight: ");
      tft.println(total_weight);
      break;
    }
    case SPOOL_MODE: {
      // Calculate filament weight and length
      float current_weight = scale.get_units() - empty_spool_weight;
      float filament_length = current_weight / (filament_density * PI * (filament_diameter / 2) * (filament_diameter / 2));

      // Display information
      tft.setCursor(0, 0);
      tft.println("Spool Mode:");
      tft.print("Empty Spool Weight: ");
      tft.println(empty_spool_weight);
      tft.print("Filament Weight: ");
      tft.println(current_weight);
      tft.print("Filament Length: ");
      tft.println(filament_length);
      break;
    }
  }
}

void switchMode() {
  // Toggle between modes on long press
  if (currentState == CALIBRATION_MODE) {
    currentState = SPOOL_MODE;
  } else {
    currentState = CALIBRATION_MODE;
  }
  Serial.print("Switched to Mode: ");
  Serial.println(currentState == CALIBRATION_MODE ? "Calibration Mode" : "Spool Mode");
}

void applyNewCalibrationFactor() {
  // Apply new calibration factor on short press
  calibration_factor = new_calibration_factor;
  scale.set_scale(calibration_factor); // Apply new calibration factor
  Serial.print("New Calibration Factor Applied: ");
  Serial.println(calibration_factor);
}
