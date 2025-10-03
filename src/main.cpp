#include "HX711.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <OneButton.h>

// Pin definitions for the load cell (ESP32)
#define LOADCELL_DOUT_PIN 32
#define LOADCELL_SCK_PIN 33

// Pin definitions for the rotary encoder (ESP32)
#define ENCODER_CLK_PIN 16
#define ENCODER_DT_PIN 17
#define ENCODER_SW_PIN 19

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
float calibration_factor = -700.0;
float new_calibration_factor = calibration_factor;  // New calibration factor to be adjusted

// Filament details
float empty_spool_weight = 250.0; // in grams (default value)
float new_empty_spool_weight = empty_spool_weight; // Initialize with current value
float previous_empty_spool_weight = empty_spool_weight;
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
  SPOOL_MODE,
  MTSPOOL_MODE
};

// Initial state of the state machine (default to SPOOL_MODE)
State currentState = SPOOL_MODE;

// --- display caching to reduce flicker ---
float previous_display_weight = NAN;
float previous_display_length = NAN;
State previous_display_state = (State)-1;
const float WEIGHT_CHANGE_THRESHOLD = 0.05f;  // grams
const float LENGTH_CHANGE_THRESHOLD = 0.01f;  // meters

// calibration-mode specific cache to avoid redraws
float previous_calibration_factor = NAN;
float previous_new_calibration_factor = NAN;
// replaced single total cache with separate current/new caches
float previous_total_weight_current = NAN;
float previous_total_weight_new = NAN;
const float CAL_FACTOR_DELTA = 0.0005f;
const float TOTAL_WEIGHT_DELTA = 0.05f;

// Variables to track last update time and display changes
unsigned long lastUpdateMillis = 0;
const unsigned long UPDATE_TIMEOUT = 30000; // 30 seconds
bool displayChanged = false;

// Constants for smoothing
const int SMA_WINDOW_SIZE = 5; // Number of readings to average
float weightBuffer[SMA_WINDOW_SIZE];
float lengthBuffer[SMA_WINDOW_SIZE];
int bufferIndex = 0;

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
  tft.setRotation(1);         // Rotate the screen by 270 degrees
  tft.fillScreen(ST7735_BLACK);
  tft.setTextColor(ST7735_WHITE);

  // Initialize the encoder button
  encoderButton.attachClick(applyNewCalibrationFactor);
  encoderButton.attachLongPressStart(switchMode);

  // Initialize SMA buffers
  for (int i = 0; i < SMA_WINDOW_SIZE; i++) {
    weightBuffer[i] = 0.0;
    lengthBuffer[i] = 0.0;
  }

  Serial.println("Load cell and encoder initialized. Place the spool on the load cell.");
}

void adjustEmptySpoolWeight() {
  // Adjust new_empty_spool_weight using encoder
  new_empty_spool_weight += encoderPos; // Example adjustment
  encoderPos = 0; // Reset encoder position after adjustment

  if (currentState == MTSPOOL_MODE) {
    applyNewCalibrationFactor(); // Save the new weight value when in MTSPOOL_MODE
  }
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
      //adjustSpoolWeight();
      break;
    case MTSPOOL_MODE: {
      // Handle MTSPOOL mode
      adjustEmptySpoolWeight();
      break;
    }
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
      encoderPos--;
    } else {
      encoderPos++;
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

void updateDisplay() {
  // Only do a full clear when the mode changes
  if (currentState != previous_display_state) {
    tft.fillScreen(ST7735_BLACK);
    previous_display_weight = NAN;
    previous_display_length = NAN;
    previous_display_state = currentState;
    // reset calibration caches too so first entry draws everything
    previous_calibration_factor = NAN;
    previous_new_calibration_factor = NAN;
    previous_total_weight_current = NAN;
    previous_total_weight_new = NAN;
  }

  tft.setTextWrap(false); // avoid wrapping side-effects

  switch (currentState) {
    case CALIBRATION_MODE: {
      // read units using the currently-applied calibration
      float units_current = scale.get_units();
      // total as reported with the current factor
      float total_current = units_current;
      // estimate total using the new calibration factor (avoid divide-by-zero)
      float total_new = total_current;
      if (new_calibration_factor != 0.0f) {
        total_new = units_current * (calibration_factor / new_calibration_factor);
      }

       // Only redraw small value areas when the value actually changes.
       // Header/top label drawn only on mode change above (screen was cleared).
       tft.setTextSize(1);
       tft.setTextColor(ST7735_WHITE, ST7735_BLACK);

       // Cur Factor
       if (isnan(previous_calibration_factor) || fabs(calibration_factor - previous_calibration_factor) > CAL_FACTOR_DELTA) {
         char buf1[32];
         snprintf(buf1, sizeof(buf1), "Cur Factor: %5.0f", calibration_factor);
         tft.setCursor(0, 12);
         tft.print(buf1);
         previous_calibration_factor = calibration_factor;
       }

       // New Factor
       if (isnan(previous_new_calibration_factor) || fabs(new_calibration_factor - previous_new_calibration_factor) > CAL_FACTOR_DELTA) {
         char buf2[32];
         snprintf(buf2, sizeof(buf2), "New Factor: %5.0f", new_calibration_factor);
         tft.setCursor(0, 28);
         tft.print(buf2);
         previous_new_calibration_factor = new_calibration_factor;
       }

      // Total with current factor
      if (isnan(previous_total_weight_current) || fabs(total_current - previous_total_weight_current) > TOTAL_WEIGHT_DELTA) {
        char buf3[32];
        snprintf(buf3, sizeof(buf3), "Total (cur): %6.2f g  ", total_current);
        tft.setCursor(0, 44);
        tft.print(buf3);
        previous_total_weight_current = total_current;
      }
      // Estimated total with new factor
      if (isnan(previous_total_weight_new) || fabs(total_new - previous_total_weight_new) > TOTAL_WEIGHT_DELTA) {
        char buf4[32];
        snprintf(buf4, sizeof(buf4), "Total (new): %6.2f g  ", total_new);
        tft.setCursor(0, 56);
        tft.print(buf4);
        previous_total_weight_new = total_new;
      }

       break;
     }

    case SPOOL_MODE: {
      // Read raw weight and compute filament values
      float raw_weight = scale.get_units();
      float current_weight = raw_weight - empty_spool_weight;

      if (current_weight < 0) {
        tft.fillScreen(ST7735_BLACK); // Blank the screen if current weight is negative
        displayChanged = true; // Set flag to indicate display changed
        break;
      }

      float filament_length = 0.0f;
      // filament_diameter in mm -> convert to cm for g/cm^3 density
      float radius_cm = (filament_diameter / 10.0f) / 2.0f;
      float area_cm2 = PI * radius_cm * radius_cm;
      float volume_cm3 = 0;
      if (filament_density > 0) volume_cm3 = current_weight / filament_density;
      float length_m = 0;
      if (area_cm2 > 0) length_m = (volume_cm3 / area_cm2) / 100.0f;
      filament_length = length_m;

      // Only update moving numbers if they changed enough
      bool weight_changed = isnan(previous_display_weight) || fabs(previous_display_weight - current_weight) >= WEIGHT_CHANGE_THRESHOLD;
      bool length_changed = isnan(previous_display_length) || fabs(previous_display_length - filament_length) >= LENGTH_CHANGE_THRESHOLD;

      if (weight_changed) {
        // Clear a larger area for the big weight text to avoid ghosting
        tft.fillRect(0, 16, tft.width(), 72, ST7735_BLACK);
        tft.setTextSize(3);            
        tft.setCursor(0, 16);
        tft.setTextColor(ST7735_WHITE, ST7735_BLACK);
        char wbuf[24];
        // show filament mass in grams without label
        snprintf(wbuf, sizeof(wbuf), "%7.2f g", current_weight);
        tft.print(wbuf);
        previous_display_weight = current_weight;
      }

      if (length_changed) {
        // Clear only the rectangle used for the length value
        tft.fillRect(0, 88, tft.width(), 40, ST7735_BLACK);
        tft.setTextSize(3);              
        tft.setCursor(0, 88);
        tft.setTextColor(ST7735_WHITE, ST7735_BLACK);
        char lbuf[32];
        // show filament length in meters without label
        snprintf(lbuf, sizeof(lbuf), "%7.2f m", filament_length);
        tft.print(lbuf);
        previous_display_length = filament_length;
      }

      break;
    }
    case MTSPOOL_MODE: {
      // Display current and new empty_spool_weight
      //if (isnan(previous_empty_spool_weight) || fabs(empty_spool_weight - previous_empty_spool_weight) > WEIGHT_CHANGE_THRESHOLD) {
        char buf1[32];
        snprintf(buf1, sizeof(buf1), "Empty Spool Weight: %6.2f g", empty_spool_weight);
        tft.setCursor(0, 12);
        tft.print(buf1);

        char buf2[32];
        snprintf(buf2, sizeof(buf2), "New Spool Weight: %6.2f g", new_empty_spool_weight);
        tft.setCursor(0, 28);
        tft.print(buf2);

        previous_empty_spool_weight = empty_spool_weight;
      //}

      break;
    }
  }
}

void switchMode() {
  // Toggle between modes on long press
  if (currentState == CALIBRATION_MODE) {
    currentState = MTSPOOL_MODE;
  } else if (currentState == MTSPOOL_MODE) 
  {
    currentState = SPOOL_MODE;
  } else {  
    currentState = CALIBRATION_MODE;
  }
  Serial.print("Switched to Mode: ");
  Serial.println(currentState == CALIBRATION_MODE ? "Calibration Mode" : "Spool Mode");
}

void applyNewCalibrationFactor() {
  // Apply new calibration factor on short press
  empty_spool_weight = new_empty_spool_weight; // Save the new weight value
}
