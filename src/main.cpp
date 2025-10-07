#include "HX711.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <OneButton.h>
#include <Preferences.h>
  #include <nvs_flash.h>

// HX711 pins
#define DT  32
#define SCK 33

HX711 scale;

// Pin definitions for the rotary encoder (ESP32)
#define ENCODER_CLK 16
#define ENCODER_DT 17
#define ENCODER_SW 19

// Pin definitions for the ST7735 (ESP32)
#define TFT_CS     5
#define TFT_RST    15
#define TFT_DC     2
#define TFT_MOSI   23
#define TFT_SCK    18

int ticks = 0;

volatile int calibrationFactor = -1198; // adjustable with encoder
volatile long calibrationOffset = 55076;
volatile long lastEncoderCLK = LOW;
volatile bool tareRequested = false;

// Debounce variables
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 4; // milliseconds

// Preferences namespace and key for storing empty spool weight
#define PREFERENCES_NAMESPACE "storage"
#define EMPTY_SPOOL_WEIGHT_KEY "mtsw"
Preferences preferences;

// TFT display setup
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCK, TFT_RST);

// Filament details
int empty_spool_weight = 250; // in grams (default value)
const float filament_density = 1.24;    // in g/cm^3 for PLA (adjust for your material)
const float filament_diameter = 1.75;   // in mm

// Threshold values for display updates
const float weightThreshold = 0.1;
const float lengthThreshold = 0.1;

static float lastWeight = -1.0;
static float lastLength = -1.0;
static float lastSpoolWeight = -1.0;

// Rolling average variables
const int rollingAverageSize = 5;
float weightBuffer[rollingAverageSize];
int bufferIndex = 0;

unsigned long lastSerialUpdate = 0; // Timer variable to track the last update time
unsigned long lastDisplayUpdate = 0; // Timer variable to track the last display update

// Configurable variable for timer interval
const unsigned long weightReadInterval = 500; // milliseconds (0.5 seconds)
const unsigned long blankScreenDelay = 30000; // milliseconds (30 seconds)

// Function to calculate filament length
float calculateFilamentLength(float weight) {
  float radius_cm = (filament_diameter / 20.0); // mm → cm, then /2 for radius
  float area_cm2 = PI * radius_cm * radius_cm;
  float volume_cm3 = weight / filament_density;
  float res = volume_cm3 / area_cm2 / 100;  // length in m
  if (res < 0) {
    return 0;
  }
  return res;
}

void IRAM_ATTR encoderISR() {
  // Handle the encoder state change
  static long lastStateA = HIGH;
  static long lastStateB = HIGH;

  long currentStateA = digitalRead(ENCODER_CLK);
  long currentStateB = digitalRead(ENCODER_DT);
  if ((currentStateA != lastStateA) || (currentStateB != lastStateB)) {
    if ((currentStateA == LOW) && (lastStateA == HIGH)) {
      if (currentStateB == LOW) {
        empty_spool_weight--; // Clockwise rotation
      } else {
        empty_spool_weight++; // Counterclockwise rotation
      }
    }

    lastDebounceTime = millis();
  }

  lastStateA = currentStateA;
  lastStateB = currentStateB;
}

void handleSingleClick() {
  tft.fillScreen(ST7735_GREEN);
  Serial.println("TARE");
  scale.tare(); // Tare the scale
}

void handleLongPress() {
    preferences.begin(PREFERENCES_NAMESPACE, false);

  if (preferences.putInt(EMPTY_SPOOL_WEIGHT_KEY, empty_spool_weight)) {
     preferences.end();
    Serial.println("Saved empty spool weight: " + String(empty_spool_weight) + " g")  ; // Save to Preferences
  tft.fillScreen(ST7735_BLUE);
}
  preferences.end();
}

void loadEmptySpoolWeight() {
  if (preferences.isKey(EMPTY_SPOOL_WEIGHT_KEY)) {
    empty_spool_weight = preferences.getInt(EMPTY_SPOOL_WEIGHT_KEY, 250); // Load from Preferences
    Serial.println("Loaded empty spool weight: " + String(empty_spool_weight) + " g");
  } else {
    empty_spool_weight = 250; // Default value
    Serial.println("No saved empty spool weight found, using default: " + String(empty_spool_weight) + " g");
  }
  //Serial.println("Loaded empty spool weight: " + String(empty_spool_weight) + " g");
}

OneButton button(ENCODER_SW, true);

unsigned long lastWeightReadTime = millis(); // Declare and initialize lastWeightReadTime
unsigned long lastButtonTickTime = 0; // Timer variable to track the last button tick time

void setup() {
  Serial.begin(115200);
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      Serial.println("Erasing NVS...");
      nvs_flash_erase();
      nvs_flash_init();
  }
  Serial.println("NVS initialized.");

//Preferences preferences;

  // HX711 init
  scale.begin(DT, SCK);
  // Encoder pins
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENCODER_DT), encoderISR, CHANGE);
  scale.set_scale(calibrationFactor); // start with default
  scale.set_offset(calibrationOffset);
  lastWeightReadTime = millis(); // Initialize lastWeightReadTime
  
  preferences.begin(PREFERENCES_NAMESPACE, false);
  empty_spool_weight = preferences.getInt(EMPTY_SPOOL_WEIGHT_KEY, 0);
  //loadEmptySpoolWeight(); // Load empty_spool_weight from Preferences
  Serial.println("Loaded empty spool weight: " + String(empty_spool_weight) + " g");
  preferences.end();
  lastSpoolWeight = empty_spool_weight;
  // Initialize TFT display
  tft.initR(INITR_BLACKTAB);  // Initialize a ST7735S chip, black tab
  tft.setRotation(1);         // Rotate the screen by 270 degrees
  tft.fillScreen(ST7735_BLACK);
  tft.setTextColor(ST7735_WHITE);

  // Initialize weight buffer
  for (int i = 0; i < rollingAverageSize; i++) {
    weightBuffer[i] = 0.0;
  }

  lastWeightReadTime = millis(); // Initialize the last weight read time
  lastDisplayUpdate = millis(); // Initialize the last display update time

  // Attach button events
  button.attachClick(handleSingleClick);
  button.attachLongPressStart(handleLongPress);
  Serial.println("Setup complete.");
}

void loop() {
  unsigned long currentTime = millis();

    button.tick();
  // --- Read weight ---
  if (currentTime - lastWeightReadTime >= weightReadInterval) {
    if (scale.is_ready()) {
      //Serial.println("HX711 ready");
      float newWeight = scale.get_units(1); // average of 5 reads

      // Add new weight to buffer and calculate average
      weightBuffer[bufferIndex] = newWeight;
      bufferIndex = (bufferIndex + 1) % rollingAverageSize;
      float totalWeight = 0.0;
      for (int i = 0; i < rollingAverageSize; i++) {
        totalWeight += weightBuffer[i];
      }
      float averageWeight = (totalWeight / rollingAverageSize) - empty_spool_weight; // Subtract empty spool weight
//Serial.print("Average Weight: ");
//Serial.println(averageWeight);
      // Calculate filament length
      float filamentLength = calculateFilamentLength(averageWeight);
//Serial.print("Filament Length: ");
//Serial.println(filamentLength); 
      if (averageWeight <= 0) {
        tft.fillScreen(ST7735_BLACK); // Clear the screen if weight is negative
        //Serial.println("NO WEIGHT");
        lastWeight = -1.0; // Reset lastWeight to force update when weight is positive again
        lastLength = -1.0; // Reset lastLength to force update when weight is positive again      
      } else {

        // Update display only when necessary
        if (abs(averageWeight - lastWeight) > weightThreshold) {
          tft.fillRect(10, 20, tft.width(), 40, ST7735_BLACK); // Clear the area where weight is displayed
          tft.setTextSize(3);
          tft.setCursor(10, 20);
          char buf[16];
          snprintf(buf, sizeof(buf), "%4.1f g", averageWeight);
          tft.print(buf);
          lastWeight = averageWeight;
          Serial.println("WEIGHT");
        }
        if (abs(filamentLength - lastLength) > lengthThreshold) {
          Serial.print("LENGTH ");
          Serial.println(abs(filamentLength - lastLength));
          tft.fillRect(10, 60, tft.width(), 40, ST7735_BLACK); // Clear the area where filament length is displayed
          tft.setTextSize(3);
          tft.setCursor(10, 60);
          char buf2[16];
          snprintf(buf2, sizeof(buf2), "%4.1f m", filamentLength); 
          tft.print(buf2);
          lastLength = filamentLength;
        }
      }

      // Check if one second has passed since the last update
      if (currentTime - lastSerialUpdate >= 1000) {
        Serial.printf("Weight: %.1f g, Length: %.2f m %d g spool\n", averageWeight, filamentLength, empty_spool_weight);
        lastSerialUpdate = currentTime;
      }
    
   
  } else {
    //Serial.print("HX711 not ready: ");
  }
 lastWeightReadTime = currentTime; // Update the last weight read time
  // Display empty spool weight if it has changed
  if (empty_spool_weight != lastSpoolWeight) {
    tft.fillRect(0, 120, tft.width(), 20, ST7735_BLACK); // Clear the area where spool weight is displayed
    tft.setTextSize(1); // Set text size to 1
    char buf3[16];
    snprintf(buf3, sizeof(buf3), " Spool: %d g", empty_spool_weight);
    tft.setCursor(0, 120);
    tft.print(buf3);
    lastSpoolWeight = empty_spool_weight;
  }

  // Blank the screen if it goes for 30 seconds without being updated
  if (currentTime - lastDisplayUpdate > blankScreenDelay) {
    tft.fillScreen(ST7735_BLACK); // Clear the screen
    Serial.println("BLANK SCREEN");
    lastDisplayUpdate = currentTime; // Reset the display update time
  }
}
}

