#include <U8g2lib.h>
//U8X8_SSD1306_128X64_NONAME_HW_I2C u8x8(/* reset=*/ U8X8_PIN_NONE);
U8X8_SH1106_128X64_NONAME_HW_I2C u8x8(/* reset=*/U8X8_PIN_NONE);

#include <Keypad.h>
#include <Wire.h>  // Add this at the top with your other includes

#define USE_EXTERNAL_POWER true  // Set to true when using external power

const byte ROWS = 4;  // four rows
const byte COLS = 4;  // four columns
// define the cymbols on the buttons of the keypads

// char keys[ROWS][COLS] = {
//  { '1', '2', '3', 'A' },
//  { '4', '5', '6', 'B' },
//  { '7', '8', '9', 'C' },
//  { '*', '0', '#', 'D' }
//};

char keys[ROWS][COLS] = {
    {'D', '#', '0', '*'},
    {'C', '9', '8', '7'},
    {'B', '6', '5', '4'},
    {'A', '3', '2', '1'}};

// matrix
// byte colPins[COLS] = { 7, 6, 5, 4 };    //connect to the column pinouts of the keypad
// byte rowPins[ROWS] = { 11, 10, 9, 8 };  //connect to the row pinouts of the keypad

// keypad
byte colPins[COLS] = {4, 5, 6, 7};    // connect to the column pinouts of the keypad
byte rowPins[ROWS] = {8, 9, 10, 11};  // connect to the row pinouts of the keypad

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

#include <Adafruit_Fingerprint.h>
#if (defined(__AVR__) || defined(ESP8266)) && !defined(__AVR_ATmega2560__)
// For UNO and others without hardware serial, we must use software serial...
// pin #2 is IN from sensor (GREEN wire)
// pin #3 is OUT from arduino  (WHITE wire)
// Set up the serial port to use softwareserial..
SoftwareSerial mySerial(2, 3);

#else
// On Leonardo/M0/etc, others with hardware serial, use hardware serial!
// #0 is green wire, #1 is white
#define mySerial Serial1

#endif

#define ACCESS_DELAY 5000  // 5 seconds unlock time

Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

const int LED_GN = A0;
const int LED_RD = A1;
const int RELAY = 12;
const int BUZZER = 13;

bool passwdOk = false;
bool passwdOn = false;
bool enrollOn = false;
bool on = false;
char key;

void setup() {
  // Critical for external power stabilization
  delay(300);  // Allow power supply to fully stabilize

  // Configure pins for optimal external power operation
  pinMode(RELAY, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(LED_GN, OUTPUT);
  pinMode(LED_RD, OUTPUT);
  pinMode(A2, OUTPUT);
  pinMode(A3, OUTPUT);

  // Immediately set pins to known states - KEEP RELAY LOCKED
  digitalWrite(RELAY, HIGH);  // HIGH = locked
  digitalWrite(BUZZER, LOW);
  digitalWrite(LED_GN, LOW);
  digitalWrite(LED_RD, HIGH);
  digitalWrite(A2, LOW);
  digitalWrite(A3, LOW);

  // Allow system to stabilize with external power
  delay(100);

  randomSeed(analogRead(0));

  // Initialize serial at standard baud rate
  Serial.begin(9600);

  // Initialize I2C with proper timing for external power
  Wire.begin();
  delay(50);  // Give I2C time to stabilize with external power

  // Initialize OLED with appropriate settings
  u8x8.begin();
  u8x8.setContrast(70);  // Increased contrast for better visibility with external power
  u8x8.setFlipMode(2);
  u8x8.setFont(u8x8_font_torussansbold8_r);
  u8x8.clearDisplay();
  u8x8.drawString(0, 0, "Starting...");

  // REMOVED: Test relay with SHORT pulse - prevents power spike issues
  // REMOVED: digitalWrite(RELAY, LOW);  // Briefly activate relay
  // REMOVED: delay(200);               // Very brief activation
  // REMOVED: digitalWrite(RELAY, HIGH); // Back to locked state

  // Initialize fingerprint sensor with fixed baud rate
  delay(100);  // Allow power to stabilize for sensor
  finger.begin(57600);
  delay(10);   // Short delay like in working code

  // Display ready message
  u8x8.clearDisplay();
  u8x8.drawString(0, 0, "System Ready");
  u8x8.drawString(0, 1, "Ext Power Mode");
  delay(2000);
  u8x8.clearDisplay();

  // Check fingerprint sensor
  if (!finger.verifyPassword()) {
    u8x8.drawString(0, 0, "Sensor not found");
    delay(2000);  // Show error but continue
  }

  // Load parameters
  readingParamters();
}

void loop() {
  // Get keypad input with debounce for external power
  static unsigned long lastKeyCheck = 0;
  if (millis() - lastKeyCheck > 50) {  // 50ms debounce
    lastKeyCheck = millis();
    key = keypad.getKey();
  }

  // Handle system functions with power considerations
  passwd();
  enroll();
  search();

  // Add a small delay to prevent CPU from running too hot
  // This is actually beneficial for power stability
  delay(10);
}

void passwd() {
  if (passwdOk) return;

  const char password[] = "1234";

  static char passw[9] = "";  // Smaller array - 8 chars + null terminator
  static byte index = 0;
  byte size = sizeof(passw);

  if (key) {
    if (passwdOn) {
      switch (key) {
        default:
          if (index < size - 1) {
            passw[index] = key;
            index++;
          }
          break;

        case '*':
          if (index > 0) {
            index--;
            passw[index] = 0;
          }
          break;

        case '#':
          if (!strcmp(passw, password)) passwdOk = true;
          if (passwdOk)
            u8x8.drawString(0, 6, "password correct");
          else
            u8x8.drawString(0, 6, " password wrong ");
          index = 0;
          memset(passw, 0, size);
          delay(1000);
          u8x8.clearDisplay();
          passwdOn = false;
          return;
      }
    }
    passwdOn = true;

    u8x8.drawString(0, 0, "      login     ");
    u8x8.drawString(0, 1, "----------------");
    u8x8.drawString(0, 2, " enter password ");
    u8x8.clearLine(3);
    u8x8.drawString(0, 4, "****************");
    u8x8.drawString(0, 4, passw);
    u8x8.clearLine(5);
    u8x8.clearLine(6);
    u8x8.drawString(0, 7, "----------------");
  }
}

void enroll() {
  if (!passwdOk) return;

  static uint8_t id = 0;
  static unsigned long code = 0;
  static unsigned long rand = random(1000, 9999);  // Smaller random number to save memory

  int num;

  if (key) {
    if (enrollOn) {
      switch (key) {
        case '0' ... '9':

          num = key - 48;

          if (!code) {
            if (code + num <= 99999999) code = code + num;
          } else {
            if (10 * code + num <= 99999999) code = 10 * code + num;
          }
          break;

        case '*':
          code = code / 10;
          break;

        case 'A':
          u8x8.clearDisplay();
          if (code >= 0 & code <= 255) {
            id = code;
            getFingerprintEnroll(id);
          } else
            u8x8.drawString(0, 0, "only 0 - 255");
          delay(3000);
          break;

        case 'B':
          u8x8.clearDisplay();
          if (code >= 0 & code <= 255) {
            id = code;
            deleteFingerprint(id);
          } else
            u8x8.drawString(0, 0, "only 0 - 255");
          delay(3000);
          break;

        case 'C':
          u8x8.clearDisplay();
          if (code == rand) {
            finger.emptyDatabase();
            rand = random(10000000, 99999999);
            u8x8.drawString(0, 0, "-database empty-");
          } else {
            u8x8.drawString(0, 0, "only");
            u8x8.setCursor(5, 0);
            u8x8.print(rand);
          }
          delay(3000);
          break;

        case 'D':
          on = !on;
          break;

        case '#':
          u8x8.clearDisplay();
          passwdOk = false;
          enrollOn = false;
          on = false;
          return;
      }
    }
    enrollOn = true;

    u8x8.drawString(0, 0, "                ");
    u8x8.setCursor(0, 0);
    u8x8.print(code);
    u8x8.drawString(0, 1, "----------------");
    u8x8.drawString(0, 2, "A: enroll finger");
    u8x8.drawString(0, 3, "B: delete finger");
    u8x8.drawString(0, 4, "C: ClearDatabase");
    u8x8.drawString(0, 5, "D: <open><close>");
    u8x8.drawString(0, 6, "#:    logout    ");
    u8x8.drawString(0, 7, "----------------");
  }
}

void search() {
  const unsigned long ONTIME = 10000;
  const unsigned long PERIOD = 15000;
  const unsigned long REPEAT = 2000;

  unsigned long timer = millis();
  static unsigned long period = timer - PERIOD;
  static unsigned long repeat = timer - REPEAT;
  int p;

  if (!enrollOn) {
    if (timer - period > PERIOD) {
      if (timer - repeat > REPEAT) {
        if (passwdOn)
          p = getFingerprintIDez();
        else
          p = getFingerprintID();

        timer = millis();
        repeat = timer;
        switch (p) {
          default:
            break;
          case 0 ... 255:
            period = timer;
            // Call activateRelay() instead of direct relay control
            activateRelay();
            break;
        }
      }
    }
    if (timer - period > 1000) {
      digitalWrite(BUZZER, LOW);
      if (timer - period > ONTIME) {
        digitalWrite(RELAY, HIGH);  // CORRECTED: Set to HIGH to lock (matches activateRelay)
        digitalWrite(LED_RD, HIGH);
        if (timer - period < PERIOD) {
          digitalWrite(LED_GN, LOW);
        } else {
          digitalWrite(LED_GN, bitRead(timer, 10));
        }
      }
    }
    return;
  }

  period = timer - PERIOD;

  if (on) {
    digitalWrite(RELAY, LOW);   // CORRECTED: Set to LOW to unlock (matches activateRelay)
    digitalWrite(BUZZER, HIGH);
    digitalWrite(LED_RD, LOW);
    digitalWrite(LED_GN, HIGH);
  } else {
    digitalWrite(RELAY, HIGH);  // CORRECTED: Set to HIGH to lock (matches activateRelay)
    digitalWrite(BUZZER, LOW);
    digitalWrite(LED_RD, HIGH);
    digitalWrite(LED_GN, LOW);
  }
}

void readingParamters() {
  u8x8.clearDisplay();
  u8x8.drawString(0, 0, "ReadingParameter");
  finger.getParameters();
  u8x8.drawString(0, 1, "Status: 0x");
  u8x8.setCursor(10, 1);
  u8x8.print(finger.status_reg, HEX);
  u8x8.drawString(0, 2, "Sys ID: 0x");
  u8x8.setCursor(10, 2);
  u8x8.print(finger.system_id, HEX);
  u8x8.drawString(0, 3, "Capacity:");
  u8x8.setCursor(10, 3);
  u8x8.print(finger.capacity);
  u8x8.drawString(0, 4, "Securitylevel:");
  u8x8.setCursor(15, 4);
  u8x8.print(finger.security_level);
  u8x8.drawString(0, 5, "address:");
  u8x8.setCursor(8, 5);
  u8x8.print(finger.device_addr, HEX);
  u8x8.drawString(0, 6, "Packet len:");
  u8x8.setCursor(12, 6);
  u8x8.print(finger.packet_len);
  u8x8.drawString(0, 7, "Baudrate:");
  u8x8.setCursor(10, 7);
  u8x8.print(finger.baud_rate);
  delay(3000);
  u8x8.clearDisplay();
}

uint8_t getFingerprintEnroll(uint8_t id) {
  int i;
  int p;
  unsigned long timeout = millis();  // Remove "static" - causes memory issues

  u8x8.clearDisplay();

  u8x8.drawString(0, 0, "Enrolling ID");
  u8x8.setCursor(13, 0);
  u8x8.print(id);

  for (i = 1; i <= 2; i++) {
    switch (i) {
      case 1:
        u8x8.drawString(0, 2, "Waiting f.finger");
        break;
      case 2:
        u8x8.drawString(0, 2, "Put same finger");
        break;
    }

    timeout = millis();
    do {
      p = finger.getImage();
      switch (p) {
        case FINGERPRINT_OK:
          u8x8.drawString(0, 3, "Image taken");
          break;
        case FINGERPRINT_NOFINGER:
          u8x8.drawString(0, 3, "...");
          break;
        case FINGERPRINT_PACKETRECIEVEERR:
          u8x8.drawString(0, 3, "Communicat.error");
          break;
        case FINGERPRINT_IMAGEFAIL:
          u8x8.drawString(0, 3, "Imaging error");
          break;
        default:
          u8x8.drawString(0, 3, "Unknown error");
          break;
      }
      if (millis() - timeout > 60000) return p;
    } while (p != FINGERPRINT_OK);

    // OK success!

    p = finger.image2Tz(i);
    switch (p) {
      case FINGERPRINT_OK:
        u8x8.drawString(0, 4, "Image converted");
        break;
      case FINGERPRINT_IMAGEMESS:
        u8x8.drawString(0, 4, "Image too messy");
        return p;
      case FINGERPRINT_PACKETRECIEVEERR:
        u8x8.drawString(0, 4, "Communicat.error");
        return p;
      case FINGERPRINT_FEATUREFAIL:
        u8x8.drawString(0, 4, "Features fail");
        return p;
      case FINGERPRINT_INVALIDIMAGE:
        u8x8.drawString(0, 4, "Invalid Image");
        return p;
      default:
        u8x8.drawString(0, 4, "Unknown error");
        return p;
    }

    // OK converted!

    u8x8.drawString(0, 6, "Remove finger");
    delay(2000);

    timeout = millis();
    do {
      p = finger.getImage();
      if (millis() - timeout > 60000) return p;
    } while (p != FINGERPRINT_NOFINGER);

    u8x8.clearLine(2);
    u8x8.clearLine(3);
    u8x8.clearLine(4);
    u8x8.clearLine(5);
    u8x8.clearLine(6);
    u8x8.clearLine(7);
  }

  u8x8.drawString(0, 2, "Creating model!");

  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    u8x8.drawString(0, 3, "Prints matched!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    u8x8.drawString(0, 3, "Communicat.error");
    return p;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    u8x8.drawString(0, 3, "Prints not match");
    return p;
  } else {
    u8x8.drawString(0, 3, "Unknown error");
    return p;
  }

  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    u8x8.drawString(0, 4, "Stored!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    u8x8.drawString(0, 4, "Communicat.error");
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    u8x8.drawString(0, 4, "Could not store");
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    u8x8.drawString(0, 4, "FlashWrite error");
    return p;
  } else {
    u8x8.drawString(0, 4, "Unknown error");
    return p;
  }
  return true;
}

uint8_t deleteFingerprint(uint8_t id) {
  uint8_t p = -1;

  u8x8.clearDisplay();

  p = finger.deleteModel(id);

  if (p == FINGERPRINT_OK) {
    u8x8.drawString(0, 0, "Deleted ID");
    u8x8.setCursor(11, 0);
    u8x8.print(id);
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    u8x8.drawString(0, 0, "Communicat.error");
  } else if (p == FINGERPRINT_BADLOCATION) {
    u8x8.drawString(0, 0, "Could not delete");
  } else if (p == FINGERPRINT_FLASHERR) {
    u8x8.drawString(0, 0, "FlashWrite error");
  } else {
    u8x8.drawString(0, 0, "Unknown error");
    u8x8.drawString(0, 1, "Errorcode: 0x");
    u8x8.setCursor(13, 1);
    u8x8.print(p, HEX);
  }
  return p;
}

int getFingerprintID() {
  uint8_t p;

  u8x8.clearDisplay();

  p = finger.getImage();
  switch (p) {
    case FINGERPRINT_OK:
      u8x8.drawString(0, 0, "Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      u8x8.drawString(0, 0, "No finger detect");
      return -1;
    case FINGERPRINT_PACKETRECIEVEERR:
      u8x8.drawString(0, 0, "Communicat.error");
      return -1;
    case FINGERPRINT_IMAGEFAIL:
      u8x8.drawString(0, 0, "Imaging error");
      return -1;
    default:
      u8x8.drawString(0, 0, "Unknown error");
      return -1;
  }

  // OK success!

  p = finger.image2Tz();
  switch (p) {
    case FINGERPRINT_OK:
      u8x8.drawString(0, 1, "Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      u8x8.drawString(0, 1, "Image too messy");
      return -1;
    case FINGERPRINT_PACKETRECIEVEERR:
      u8x8.drawString(0, 1, "Communicat.error");
      return -1;
    case FINGERPRINT_FEATUREFAIL:
      u8x8.drawString(0, 1, "Features fail");
      return -1;
    case FINGERPRINT_INVALIDIMAGE:
      u8x8.drawString(0, 1, "Invalid Image");
      return -1;
    default:
      u8x8.drawString(0, 1, "Unknown error");
      return -1;
  }

  // OK converted!

  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    u8x8.drawString(0, 2, "Found a match!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    u8x8.drawString(0, 2, "Communicat.error");
    return -1;
  } else if (p == FINGERPRINT_NOTFOUND) {
    u8x8.drawString(0, 2, "Did not match");
    return -1;
  } else {
    u8x8.drawString(0, 2, "Unknown error");
    return -1;
  }

  // found a match!

  u8x8.drawString(0, 3, "Found ID #");
  u8x8.setCursor(10, 3);
  u8x8.print(finger.fingerID);
  u8x8.drawString(0, 4, "Confidence:");
  u8x8.setCursor(12, 4);
  u8x8.print(finger.confidence);

  return finger.fingerID;
}

// Modify getFingerprintIDez() for power efficiency:
int getFingerprintIDez() {
  uint8_t p;

  // Power-efficient error handling
  p = finger.getImage();
  if (p != FINGERPRINT_OK) {
    delay(10);  // Small delay for power stability on error
    return -1;
  }

  // Small delay between operations for power stability
  delay(5);

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) {
    delay(10);
    return -1;
  }

  delay(5);

  p = finger.fingerSearch();
  if (p != FINGERPRINT_OK) {
    delay(10);
    return -1;
  }

  // found a match!
  return finger.fingerID;
}

void activateRelay() {
  // When using external power, we need more robust relay control

  // First ensure relay is in known state (locked)
  digitalWrite(RELAY, HIGH);
  delay(20);  // Small delay for power stabilization

  // Clear display before changing relay state to reduce power spikes
  u8x8.clearDisplay();
  delay(10);

  // UNLOCK - this is the critical part
  digitalWrite(RELAY, LOW);     // UNLOCK (active LOW)

  // Turn on indicators AFTER relay activation to avoid power sag
  delay(50);  // Allow relay to fully engage before powering other components
  digitalWrite(BUZZER, HIGH);   // Sound buzzer
  digitalWrite(LED_GN, HIGH);   // Green LED on
  digitalWrite(LED_RD, LOW);    // Red LED off

  // Display message
  u8x8.drawString(0, 0, "Access Granted");
  u8x8.drawString(0, 1, "Door Unlocked");

  // Countdown with reduced display updates
  for (int i = ACCESS_DELAY/1000; i > 0; i--) {
    // Only update every second to reduce power fluctuations
    u8x8.setCursor(0, 2);
    u8x8.print("Locks in: ");
    u8x8.print(i);
    u8x8.print("s   ");

    // Using multiple smaller delays instead of one large delay
    // This helps maintain stable power
    for (int j = 0; j < 10; j++) {
      delay(100);  // 100ms x 10 = 1 second
    }
  }

  // Clear display before changing relay state
  u8x8.clearDisplay();
  delay(10);

  // LOCK - Ensure buzzer is off BEFORE changing relay state
  digitalWrite(BUZZER, LOW);    // Buzzer off first
  delay(20);                    // Allow buzzer to stop drawing power

  // Now lock the door
  digitalWrite(RELAY, HIGH);    // LOCK (inactive HIGH)

  // Turn on indicators AFTER relay activation
  delay(50);  // Allow relay to fully engage
  digitalWrite(LED_RD, HIGH);   // Red LED on
  digitalWrite(LED_GN, LOW);    // Green LED off

  // Show locked status
  u8x8.drawString(0, 0, "Door Locked");
  u8x8.drawString(0, 1, "System Secure");
}
