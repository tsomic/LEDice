#include <LedControl.h>

#include "Buttons.h"

#define CS_PIN PB0
#define CLK_PIN PB1
#define DIN_PIN PB2
#define ROLL_PIN PB4
#define MODE_PIN A3
#define RND_PIN A0
#define ROLL_TIME 300
#define BLINK_INTERVAL 500

const uint8_t decimal[10][8] PROGMEM = {
    {B010, B101, B101, B101, B101, B101, B101, B010},
    {B100, B110, B101, B100, B100, B100, B100, B100},
    {B010, B101, B101, B100, B010, B001, B001, B111},
    {B011, B100, B100, B010, B100, B100, B100, B011},
    {B001, B001, B101, B111, B100, B100, B100, B100},
    {B111, B001, B001, B111, B100, B100, B101, B010},
    {B010, B101, B001, B011, B101, B101, B101, B010},
    {B111, B100, B100, B010, B010, B001, B001, B001},
    {B010, B101, B101, B010, B101, B101, B101, B010},
    {B010, B101, B101, B110, B100, B100, B101, B010},
};

const uint8_t dot[8][8] PROGMEM = {
    {B00000000, B00000000, B00000000, B00011000, B00011000, B00000000,
     B00000000, B00000000},
    {B11000000, B11000000, B00000000, B00000000, B00000000, B00000000,
     B00000011, B00000011},
    {B11000000, B11000000, B00000000, B00011000, B00011000, B00000000,
     B00000011, B00000011},
    {B11000011, B11000011, B00000000, B00000000, B00000000, B00000000,
     B11000011, B11000011},
    {B11000011, B11000011, B00000000, B00011000, B00011000, B00000000,
     B11000011, B11000011},
    {B11000011, B11000011, B00000000, B11000011, B11000011, B00000000,
     B11000011, B11000011},
    {B11000011, B11000011, B00000000, B11011011, B11011011, B00000000,
     B11000011, B11000011},
    {B11011011, B11011011, B00000000, B11000011, B11000011, B00000000,
     B11011011, B11011011},
};

const uint8_t smallDot[8][3] PROGMEM = {
    {B000, B010, B000}, {B100, B000, B001}, {B100, B010, B001},
    {B101, B000, B101}, {B101, B010, B101}, {B101, B101, B101},
    {B101, B111, B101}, {B111, B101, B111},
};

const uint8_t diceSidesCount[6] PROGMEM = {4, 6, 8, 10, 12, 20};

enum State { ROLLING, SELECTING };

LedControl matrix = LedControl(DIN_PIN, CLK_PIN, CS_PIN);
Buttons buttons(ROLL_PIN, MODE_PIN);

State currentState = ROLLING;
uint8_t selectedDice = 1;
uint8_t multipleNumber = 1;

bool isRolling = false;
unsigned long rollStartTime = 0;
unsigned long lastBlinkTime = 0;
bool isBlinkingOn = true;

void rollDice(bool isDisplay = false);
void rollSingleDice(uint8_t maximum, bool isDisplay = false);
void rollMultipleDice(uint8_t count, uint8_t maximum);

// start hacky random number generator
// https://forum.arduino.cc/t/the-reliable-but-not-very-sexy-way-to-seed-random/65872/52
#include <avr/eeprom.h>

void reseedRandom(uint32_t* address) {
    static const uint32_t HappyPrime = 127807;
    uint32_t raw;
    unsigned long seed;

    raw = eeprom_read_dword(address);

    do {
        raw += HappyPrime;
        seed = raw & 0x7FFFFFFF;
    } while ((seed < 1) || (seed > 2147483646));

    srandom(seed);
    eeprom_write_dword(address, raw);
}

inline void reseedRandom(unsigned short address) {
    reseedRandom((uint32_t*)(address));
}

void reseedRandomInit(uint32_t* address, uint32_t value) {
    eeprom_write_dword(address, value);
}

inline void reseedRandomInit(unsigned short address, uint32_t value) {
    reseedRandomInit((uint32_t*)(address), value);
}

uint32_t reseedRandomSeed EEMEM = 0xFFFFFFFF;
// end hacky random number generator

void setup() {
    matrix.clearDisplay(0);
    matrix.shutdown(0, false);

    uint8_t intensity = 0;
    if (!digitalRead(ROLL_PIN)) intensity += 5;
    if (!digitalRead(MODE_PIN)) intensity += 10;
    matrix.setIntensity(0, intensity);

    reseedRandom(&reseedRandomSeed);
    random(6);
    rollDice();
}

void loop() {
    buttons.update();

    switch (currentState) {
        case ROLLING:
            handleRolling(millis());

            if (buttons.wasRollReleased()) {
                startRoll();
            }
            if (buttons.wasModeReleased() && selectedDice <= 2) {
                handleModePress();
                startRoll();
            }
            if (buttons.wereBothPressed()) {
                currentState = SELECTING;
                lastBlinkTime = 0;
                isBlinkingOn = false;
                multipleNumber = 1;
            }
            break;

        case SELECTING:
            handleBlinking(millis());

            if (buttons.wereBothPressed()) {
                confirmSelection();
                rollDice();
                return;
            }

            if (buttons.wasModeReleased()) {
                selectNextDice();
                updateSelectedDice();
            } else if (buttons.wasRollReleased()) {
                selectPreviousDice();
                updateSelectedDice();
            }

            break;
    }
}

void startRoll() {
    matrix.clearDisplay(0);
    rollStartTime = millis();
    isRolling = true;
}

void handleRolling(unsigned long currentTime) {
    if (isRolling && currentTime - rollStartTime >= ROLL_TIME) {
        rollDice();
        isRolling = false;
    }
}

void handleBlinking(unsigned long currentTime) {
    if (currentTime - lastBlinkTime >= BLINK_INTERVAL) {
        lastBlinkTime = currentTime;
        isBlinkingOn = !isBlinkingOn;
        isBlinkingOn ? rollDice(true) : matrix.clearDisplay(0);
    }
}

void confirmSelection() {
    currentState = ROLLING;
    lastBlinkTime = 0;
    isBlinkingOn = false;
}

void updateSelectedDice() {
    lastBlinkTime = 0;
    isBlinkingOn = false;
}

void rollDice(bool isDisplay = false) {
    matrix.clearDisplay(0);
    uint8_t maximum = pgm_read_byte(&diceSidesCount[selectedDice]);

    if (multipleNumber == 1 || selectedDice >= 3 || isDisplay) {
        rollSingleDice(maximum, isDisplay);
    } else {
        rollMultipleDice(multipleNumber, maximum);
    }
}

void rollSingleDice(uint8_t maximum, bool isDisplay = false) {
    uint8_t number = isDisplay ? maximum : random(maximum) + 1;
    selectedDice < 3 ? rollDot(number) : rollDecimal(number);
}

void rollMultipleDice(uint8_t count, uint8_t maximum) {
    uint8_t numbers[count];

    for (uint8_t i = 0; i < count; i++) {
        numbers[i] = random(maximum);
    }

    for (uint8_t i = 0; i <= 2; i++) {
        matrix.setRow(0, 7 - i,
                      pgm_read_byte(&smallDot[numbers[0]][i]) |
                          pgm_read_byte(&smallDot[numbers[1]][i]) << 5);
    }

    if (count > 2) {
        for (uint8_t i = 0; i < 3; i++) {
            uint8_t row = pgm_read_byte(&smallDot[numbers[2]][i]);
            if (count == 4) row |= pgm_read_byte(&smallDot[numbers[3]][i]) << 5;
            matrix.setRow(0, 2 - i, row);
        }
    }
}

void rollDecimal(uint8_t number) {
    uint8_t digits[2] = {};
    int index = 1;

    while (number > 0) {
        digits[index--] = number % 10;
        number /= 10;
    }

    for (uint8_t i = 0; i < 8; i++) {
        uint8_t row =
            (digits[0] != 0) ? pgm_read_byte(&decimal[digits[0]][i]) : 0;
        row |= pgm_read_byte(&decimal[digits[1]][i]) << 5;
        matrix.setRow(0, 7 - i, row);
    }
}

void rollDot(uint8_t number) {
    for (uint8_t i = 0; i <= 7; i++) {
        matrix.setRow(0, 7 - i, pgm_read_byte(&dot[number - 1][i]));
    }
}

void selectNextDice() {
    selectedDice = (selectedDice + 1) % 6;
    multipleNumber = 1;
}

void selectPreviousDice() {
    selectedDice = (selectedDice + 5) % 6;
    multipleNumber = 1;
}

void handleModePress() { multipleNumber = (multipleNumber % 4) + 1; }