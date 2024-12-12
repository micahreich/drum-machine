#include <Arduino.h>
#include <Button2.h>
#include <Encoder.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <DrumMachineSelectionMenu.h>
#include <DrumMachineTrackData.h>
#include <DrumMachineButtons.h>
#include <DrumMachineState.h>
#include <DrumMachineLEDs.h>
#include <ShiftRegisterDebounce.h>
#include <Messaging.h>

// Drum sequence data
SequenceData sequence_data;

// LCD Parameters/pins
#define LCD_NUM_ROWS 4
#define LCD_NUM_COLS 20

LiquidCrystal_I2C lcd(0x27, LCD_NUM_COLS, LCD_NUM_ROWS);

// Rotary encoder parameters/pins
#define ENC_CLK_PIN 5
#define ENC_DT_PIN 6
#define ENC_SW_PIN 7

Encoder menu_encoder(ENC_CLK_PIN, ENC_DT_PIN);
Button2 menu_button(ENC_SW_PIN);

// Pin connections for shift registers
DrumMachineSelectionMenu selection_menu(lcd, LCD_NUM_ROWS, LCD_NUM_COLS);
long menu_encoder_ticks = 0;  // Counter for encoder ticks

// Shift register data
#define SR_LATCH_PIN 2  // PL (Pin 1) of shift registers
#define SR_CLOCK_PIN 3  // CP (Pin 2) of shift registers
#define SR_DATA_PIN 4   // Q7 (Pin 9) of the first shift register

#define NUM_SHIFT_REGISTER_BYTES 3
uint8_t shift_register_data[NUM_SHIFT_REGISTER_BYTES] = {0};

// Machine button handler
DrumMachineButtons buttons(shift_register_data);

// Action queue
DrumMachineState state;

// LEDs
#define LED_PIN_BEATS 13
#define LED_PIN_TRACKS 12

Adafruit_NeoPixel beat_strip(N_TRACK_SUBDIVISIONS, LED_PIN_BEATS, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel track_strip(N_TRACKS, LED_PIN_TRACKS, NEO_GRB + NEO_KHZ800);

DrumMachineLEDs leds(beat_strip, track_strip);

// Function prototypes
void readShiftRegister(uint8_t *data);

void setup() {
    // Set up the rotary encoder
    pinMode(ENC_CLK_PIN, INPUT);
    pinMode(ENC_DT_PIN, INPUT);
    pinMode(ENC_SW_PIN, INPUT_PULLUP);  // SW usually goes LOW when pressed, so use INPUT_PULLUP

    // Set up the shift register pins
    pinMode(SR_DATA_PIN, INPUT);
    pinMode(SR_CLOCK_PIN, OUTPUT);
    pinMode(SR_LATCH_PIN, OUTPUT);

    // Set up LEDs
    leds.init();

    // Set up the instrument menu button handler for single/long clicks
    menu_button.setDebounceTime(25);
    menu_button.setClickHandler([](Button2& btn) {
        switch (selection_menu.currPage) {
            case DrumMachineSelectionMenu::Page::CATEGORY_SELECTION:
                selection_menu.switchPage(DrumMachineSelectionMenu::INSTRUMENT_SELECTION);
                break;
            case DrumMachineSelectionMenu::Page::INSTRUMENT_SELECTION:
                selection_menu.selectInstrument();

                state.pushAction(
                    Action::create_ChangeTrackInstrumentID(state.curr_track_id, selection_menu.getSelectedInstrumentID())
                );

                break;
            case DrumMachineSelectionMenu::Page::BPM_SELECTION:
                state.pushAction(Action::create_BPMSelect(selection_menu.getCurrBPM()));

                break;
        }
    });

    menu_button.setLongClickDetectedHandler([](Button2& btn) {
        if (selection_menu.currPage == DrumMachineSelectionMenu::Page::CATEGORY_SELECTION) {
            state.pushAction(Action::create_ClearAll());
        } else {
            selection_menu.switchPage(DrumMachineSelectionMenu::CATEGORY_SELECTION);
        }
    });

    menu_button.setDoubleClickHandler([](Button2& btn) {
        selection_menu.switchPage(DrumMachineSelectionMenu::BPM_SELECTION);
    });

    selection_menu.init();

    // Set up the beat/track button handlers
    buttons.setBeatButtonRiseHandler([](int beat_idx) {
        if (selection_menu.getSelectedInstrumentID() == -1) return;

        state.pushAction(Action::create_TrackBeatToggle(state.curr_track_id, beat_idx));
    });

    buttons.setMuteUnmuteButtonRiseHandler([]() {
        state.pushAction(Action::create_TrackMuteToggle(state.curr_track_id));
    });

    buttons.setPausePlayButtonRiseHandler([]() {
        state.pushAction(Action::create_PauseToggle());
    });

    buttons.setTrackButtonRiseHandler([](int track_idx) {
        state.pushAction(Action::create_TrackSelect(track_idx));
    });

    buttons.setSampleButtonRiseHandler([]() {
        // if (selection_menu.getSelectedInstrumentID() == -1) return;

        state.pushAction(Action::create_InstrumentSample(selection_menu.getSelectedInstrumentID()));
    });

    // Set up the NeoPixel strips
    beat_strip.begin(); // Prepares the data pin NeoPixels are connected to
    beat_strip.setBrightness(128); // max brightness is 255
    beat_strip.show();  // Initialize all the pixels to 'off' since nothing has been programmed yet

    track_strip.begin(); // Prepares the data pin NeoPixels are connected to
    track_strip.setBrightness(128); // max brightness is 255
    track_strip.show();  // Initialize all the pixels to 'off' since nothing has been programmed yet

    Serial.begin(9600);
}

void loop() {
    // Update the button state
    menu_button.loop();

    // Read encoder ticks
    long newInstrumentMenuEncoderTicks = menu_encoder.read() / 4;  // Divide by 4 for detents
    if (newInstrumentMenuEncoderTicks != menu_encoder_ticks) {
        long delta = (newInstrumentMenuEncoderTicks - menu_encoder_ticks);
        menu_encoder_ticks = newInstrumentMenuEncoderTicks;

        selection_menu.loop(delta);
    }

    // Read shift register data
    readShiftRegister(shift_register_data);
    buttons.loop();

    // Loop the state
    state.loop();

    // Loop the LEDs
    leds.loop(state.sequence_data, state.curr_track_id);
}

void readShiftRegister(uint8_t *data) {
    // Latch the data from the shift registers
    digitalWrite(SR_LATCH_PIN, LOW);
    delayMicroseconds(5); // Ensure stable latch signal
    digitalWrite(SR_LATCH_PIN, HIGH);

    // Read data from each shift register
    for (int byteIndex = 0; byteIndex < NUM_SHIFT_REGISTER_BYTES; byteIndex++) {
        uint8_t byteData = 0;
        for (int bitIndex = 0; bitIndex < 8; bitIndex++) {
            byteData <<= 1;                      // Shift to make room for the next bit
            byteData |= digitalRead(SR_DATA_PIN);   // Read the current bit
            digitalWrite(SR_CLOCK_PIN, HIGH);       // Pulse the clock
            delayMicroseconds(5);
            digitalWrite(SR_CLOCK_PIN, LOW);
        }
        data[byteIndex] = byteData; // Store the byte data in the array
    }
}