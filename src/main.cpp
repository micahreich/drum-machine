#include <Arduino.h>
#include <Button2.h>
#include <Encoder.h>
#include <InstrumentSelectionMenu.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <drum_track.h>
#include <messaging.h>

#define LCD_NUM_ROWS 4
#define LCD_NUM_COLS 20

#define CLK_PIN 2
#define DT_PIN 3
#define SW_PIN 4

#define SAMPLE_PIN 5

LiquidCrystal_I2C lcd(0x27, LCD_NUM_COLS, LCD_NUM_ROWS);
Encoder instrumentMenuEncoder(CLK_PIN, DT_PIN);
Button2 instrumentMenuButton(SW_PIN);
Button2 sampleInstrumentButton(SAMPLE_PIN);

InstrumentSelectionMenu instrumentSelectionMenu(lcd, LCD_NUM_ROWS, LCD_NUM_COLS);

long instrumentMenuEncoderTicks = 0;  // Counter for encoder ticks

void setup() {
    // Set up the rotary encoder
    pinMode(CLK_PIN, INPUT);
    pinMode(DT_PIN, INPUT);
    pinMode(SW_PIN, INPUT_PULLUP);  // SW usually goes LOW when pressed, so use INPUT_PULLUP

    // Set up the sample instrument button
    pinMode(SAMPLE_PIN, INPUT_PULLUP);

    // Set up the instrument menu button handler for single/long clicks
    instrumentMenuButton.setDebounceTime(25);
    instrumentMenuButton.setClickHandler([](Button2& btn) {
        switch (instrumentSelectionMenu.currPage) {
            case InstrumentSelectionMenu::Page::CATEGORY_SELECTION:
                instrumentSelectionMenu.switchPage(InstrumentSelectionMenu::INSTRUMENT_SELECTION);
                break;
            case InstrumentSelectionMenu::Page::INSTRUMENT_SELECTION:
                instrumentSelectionMenu.selectInstrument();
                break;
        }
    });

    instrumentMenuButton.setLongClickDetectedHandler([](Button2& btn) {
        instrumentSelectionMenu.switchPage(InstrumentSelectionMenu::CATEGORY_SELECTION);
    });

    instrumentMenuButton.setDoubleClickHandler([](Button2& btn) {
        instrumentSelectionMenu.switchPage(InstrumentSelectionMenu::BPM_SELECTION);
    });

    // // Set up sample button handler
    // sample_button.setClickHandler([](Button2& btn) {
    //     Serial.println("Sample button pressed!");

    //     if (selected_instrument_id > -1) {
    //         char msg[] = {(char)selected_instrument_id};
    //         sendMessage(msg, 1, MSG_TYPE_SAMPLE);
    //     }
    // });

    instrumentSelectionMenu.init();

    Serial.begin(9600);
}

void loop() {
    // Update the button state
    instrumentMenuButton.loop();
    sampleInstrumentButton.loop();

    // Read encoder ticks
    long newInstrumentMenuEncoderTicks = instrumentMenuEncoder.read() / 4;  // Divide by 4 for detents
    if (newInstrumentMenuEncoderTicks != instrumentMenuEncoderTicks) {
        long delta = -(newInstrumentMenuEncoderTicks - instrumentMenuEncoderTicks);
        instrumentMenuEncoderTicks = newInstrumentMenuEncoderTicks;

        instrumentSelectionMenu.loop(delta);
    }
}