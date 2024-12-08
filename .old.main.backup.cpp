#include <Arduino.h>
#include <Button2.h>
#include <Encoder.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <drum_track.h>
#include <instrument_lut.h>
#include <messaging.h>

#define LCD_NUM_ROWS 4
#define LCD_NUM_COLS 20

#define CLK_PIN 2
#define DT_PIN 3
#define SW_PIN 4

#define SAMPLE_PIN 5

LiquidCrystal_I2C lcd(0x27, 20, 4);  // 0x27 is a common I2C address for LCD
Encoder encoder(CLK_PIN, DT_PIN);
Button2 button(SW_PIN);
Button2 sample_button(SAMPLE_PIN);

const byte SELECTOR_ICON = byte(0x7E);
const byte SELECTED_ICON = byte(33);

enum MenuPage {
    CATEGORY_SELECTION,
    INSTRUMENT_SELECTION,
};

MenuPage curr_page = CATEGORY_SELECTION;
int active_instrument_category_id = -1;
int selected_instrument_category_id = -1;
int selected_instrument_id = -1;
int curr_group_num = 0;

long tick_value = 0;  // Counter for encoder ticks
long relative_selection_idx = 0;
long prev_page_relative_selection_idx = 0;
long max_relative_selection_idx = NUM_CATEGORIES - 1;

// put function declarations here:
void updateLCD();
void renderInstrumentSelections(int category_id, int group_num);
void renderCategorySelections(int group_num);
void renderSelectedInstrumentIndicator(int curr_category_id, int curr_group_num);
void renderPageNumber();
void renderBrand();

int getMaxRelativeSelectionIdx();

void clearLCDCol(uint8_t col);
void clearLCDRow(uint8_t row);

void setup() {
    // Set up the rotary encoder
    pinMode(CLK_PIN, INPUT);
    pinMode(DT_PIN, INPUT);
    pinMode(SW_PIN, INPUT_PULLUP);  // SW usually goes LOW when pressed, so use INPUT_PULLUP

    pinMode(SAMPLE_PIN, INPUT_PULLUP);

    // Initialize the LCD
    lcd.init();
    lcd.backlight();

    // Set up the button handler for single click
    button.setDebounceTime(25);
    button.setClickHandler([](Button2& btn) {
        Serial.println("Button short pressed!");

        switch (curr_page) {
            case CATEGORY_SELECTION:
                // Set up variables to work for instrument selection
                curr_page = INSTRUMENT_SELECTION;
                active_instrument_category_id = relative_selection_idx;
                max_relative_selection_idx = getMaxRelativeSelectionIdx();
                prev_page_relative_selection_idx = relative_selection_idx;

                curr_group_num = 0;
                relative_selection_idx = 0;

                // Render the instrument selections
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.write(SELECTOR_ICON);

                renderInstrumentSelections(active_instrument_category_id, curr_group_num);

                break;
            case INSTRUMENT_SELECTION:
                curr_page = INSTRUMENT_SELECTION;

                int names_offset = INSTRUMENT_CAT_INFO_LUT[active_instrument_category_id][0];
                int num_instruments = INSTRUMENT_CAT_INFO_LUT[active_instrument_category_id][1];

                if (num_instruments > 0) {
                    selected_instrument_id = names_offset + relative_selection_idx;
                    selected_instrument_category_id = active_instrument_category_id;

                    long currsor_row = relative_selection_idx % LCD_NUM_ROWS;
                    clearLCDCol(0);
                    lcd.setCursor(0, currsor_row);
                    lcd.write(SELECTED_ICON);

                    Serial.print("Selecting instrument_id: ");
                    Serial.println(selected_instrument_id);
                }

                break;
        }
    });

    button.setLongClickDetectedHandler([](Button2& btn) {
        Serial.println("Button double pressed!");

        if (curr_page == INSTRUMENT_SELECTION) {
            curr_page = CATEGORY_SELECTION;
            max_relative_selection_idx = getMaxRelativeSelectionIdx();
            relative_selection_idx = prev_page_relative_selection_idx;
            prev_page_relative_selection_idx = 0;
            active_instrument_category_id = -1;

            curr_group_num = relative_selection_idx / LCD_NUM_ROWS;
            long currsor_row = relative_selection_idx % LCD_NUM_ROWS;

            lcd.clear();
            lcd.setCursor(0, currsor_row);
            lcd.write(SELECTOR_ICON);

            renderCategorySelections(curr_group_num);
        }
    });

    // Set up sample button handler
    sample_button.setClickHandler([](Button2& btn) {
        Serial.println("Sample button pressed!");

        if (selected_instrument_id > -1) {
            char msg[] = {(char)selected_instrument_id};
            sendMessage(msg, 1, MSG_TYPE_SAMPLE);
        }
    });

    renderCategorySelections(curr_group_num);
    lcd.setCursor(0, 0);
    lcd.write(SELECTOR_ICON);

    Serial.begin(9600);
}

void loop() {
    // // put your main code here, to run repeatedly:
    // char msg[0];
    // sendMessage(NULL, 0, MSG_TYPE_PAUSE);
    // delay(1000);

    // Update the button state
    button.loop();
    sample_button.loop();

    // Read encoder ticks
    long new_tick_value = encoder.read() / 4;  // Divide by 4 for detents
    if (new_tick_value != tick_value) {
        long delta = -(new_tick_value - tick_value);
        tick_value = new_tick_value;

        lcd.setCursor(0, relative_selection_idx % LCD_NUM_ROWS);
        lcd.print(" ");

        relative_selection_idx += delta;
        relative_selection_idx = max(0, min(relative_selection_idx, max_relative_selection_idx));

        // Check if we need to render a new group of instruments
        int new_group_num = relative_selection_idx / LCD_NUM_ROWS;
        if (new_group_num != curr_group_num) {
            curr_group_num = new_group_num;
            lcd.clear();

            if (curr_page == INSTRUMENT_SELECTION) {
                renderInstrumentSelections(active_instrument_category_id, curr_group_num);
            } else if (curr_page == CATEGORY_SELECTION) {
                renderCategorySelections(curr_group_num);
            }
        }

        long cursor_row = relative_selection_idx % LCD_NUM_ROWS;

        clearLCDCol(0);
        lcd.setCursor(0, cursor_row);
        lcd.write(SELECTOR_ICON);

        renderSelectedInstrumentIndicator(active_instrument_category_id, curr_group_num);

        Serial.println(relative_selection_idx);
    }
}

void renderSelectedInstrumentIndicator(int curr_category_id, int curr_group_num) {
    int names_offset = INSTRUMENT_CAT_INFO_LUT[curr_category_id][0];
    int num_instruments = INSTRUMENT_CAT_INFO_LUT[curr_category_id][1];

    int starting_idx_offset = LCD_NUM_ROWS * curr_group_num;
    int ending_idx_offset = min(num_instruments, starting_idx_offset + 4);

    if (selected_instrument_category_id == curr_category_id) {
        int selected_instrument_id_relative_idx = selected_instrument_id - names_offset;

        if (starting_idx_offset <= selected_instrument_id_relative_idx &&
            selected_instrument_id_relative_idx < ending_idx_offset) {
            lcd.setCursor(0, selected_instrument_id_relative_idx - starting_idx_offset);
            lcd.write(SELECTED_ICON);
        }
    }
}

void renderPageNumber() {
    int page_num = curr_group_num + 1;
    int max_pages = (max_relative_selection_idx / LCD_NUM_ROWS) + 1;

    String page_number_string = "P " + String(page_num) + "/" + String(max_pages);
    lcd.setCursor(LCD_NUM_COLS - page_number_string.length(), 0);
    lcd.print(page_number_string);
}

void renderBrand() {
    String brand = "DM-095";

    lcd.setCursor(LCD_NUM_COLS - brand.length(), LCD_NUM_ROWS - 1);
    lcd.print(brand);
}

void renderInstrumentSelections(int category_id, int group_num) {
    int names_offset = INSTRUMENT_CAT_INFO_LUT[category_id][0];
    int num_instruments = INSTRUMENT_CAT_INFO_LUT[category_id][1];

    renderBrand();

    if (num_instruments == 0) {
        lcd.setCursor(0, 0);
        lcd.print("No instruments!");

        return;
    }

    int starting_idx_offset = LCD_NUM_ROWS * group_num;
    int ending_idx_offset = min(num_instruments, starting_idx_offset + 4);

    for (int i = starting_idx_offset; i < ending_idx_offset; ++i) {
        lcd.setCursor(1, i - starting_idx_offset);
        lcd.print(INSTRUMENT_ID_TO_NAME_LUT[names_offset + i]);
    }

    renderSelectedInstrumentIndicator(category_id, group_num);
    renderPageNumber();
}

void renderCategorySelections(int group_num) {
    renderBrand();

    if (NUM_CATEGORIES == 0) {
        lcd.setCursor(0, 0);
        lcd.print("No categories!");

        return;
    }

    lcd.setCursor(0, 0);

    int starting_idx_offset = LCD_NUM_ROWS * group_num;
    int ending_idx_offset = min(NUM_CATEGORIES, starting_idx_offset + 4);

    for (int i = starting_idx_offset; i < ending_idx_offset; ++i) {
        lcd.setCursor(1, i - starting_idx_offset);
        lcd.print(INSTRUMENT_CAT_ID_TO_NAME_LUT[i]);
    }

    renderPageNumber();
    renderBrand();
}

int getMaxRelativeSelectionIdx() {
    switch (curr_page) {
        case CATEGORY_SELECTION:
            return NUM_CATEGORIES - 1;
        case INSTRUMENT_SELECTION:
            return INSTRUMENT_CAT_INFO_LUT[active_instrument_category_id][1] - 1;
    }

    return 0;
}

void clearLCDRow(uint8_t row) {
    lcd.setCursor(0, row);
    const char spaces[LCD_NUM_COLS] = {" "};
    lcd.print(spaces);
}

void clearLCDCol(uint8_t col) {
    for (int i = 0; i < LCD_NUM_ROWS; i++) {
        lcd.setCursor(col, i);
        lcd.print(" ");
    }
}