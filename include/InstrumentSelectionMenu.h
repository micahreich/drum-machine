#ifndef INSTRUMENT_SELECTION_MENU_H
#define INSTRUMENT_SELECTION_MENU_H

#include <Button2.h>
#include <Encoder.h>
#include <InstrumentLUT.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

#define MENU_BUTTON_DEBOUNCE_MS 25

const byte SELECTOR_ICON = byte(0x7E);
const byte SELECTED_ICON = byte(33);

const int MAX_BPM = 220;
const int MIN_BPM = 30;

class InstrumentSelectionMenu {
public:
    enum Page { CATEGORY_SELECTION, INSTRUMENT_SELECTION, BPM_SELECTION };

    Page currPage;
    int selectedInstrumentCategoryId;
    int selectedInstrumentId;

    InstrumentSelectionMenu(LiquidCrystal_I2C lcd, const int lcdNumRows, const int lcdNumCols)
        : currPage(CATEGORY_SELECTION),
          selectedInstrumentCategoryId(0),
          selectedInstrumentId(-1),
          lcd(lcd),
          lcdNumRows(lcdNumRows),
          lcdNumCols(lcdNumCols),
          relativeSelectionIdx(0),
          maxRelativeSelectionIdx(NUM_CATEGORIES - 1),
          prevPageRelativeSelectionIdx(0),
          currGroupNum(0),
          activeInstrumentCategoryId(-1),
          currBPM(120) {}

    void init() {
        lcd.init();
        lcd.backlight();

        renderCategoryList(currGroupNum);
        renderSelectionIndicator(relativeSelectionIdx);
    }

    void selectInstrument() {
        if (currPage != INSTRUMENT_SELECTION) return;

        int instrumentIdsStartingIdx = INSTRUMENT_CAT_INFO_LUT[activeInstrumentCategoryId][0];
        int numInstrumentsInCategory = INSTRUMENT_CAT_INFO_LUT[activeInstrumentCategoryId][1];

        if (numInstrumentsInCategory > 0) {
            selectedInstrumentId = instrumentIdsStartingIdx + relativeSelectionIdx;
            selectedInstrumentCategoryId = activeInstrumentCategoryId;

            clearLCDCol(0);
            renderSelectedInstrumentIndicator(selectedInstrumentId, selectedInstrumentCategoryId);
        }
    }

    void switchPage(Page newPage) {
        if (newPage == currPage) return;

        currPage = newPage;
        lcd.clear();

        switch (newPage) {
            case Page::CATEGORY_SELECTION:
                maxRelativeSelectionIdx = NUM_CATEGORIES - 1;
                activeInstrumentCategoryId = -1;
                relativeSelectionIdx = prevPageRelativeSelectionIdx;
                prevPageRelativeSelectionIdx = 0;

                currGroupNum = relativeSelectionIdx / lcdNumRows;

                renderSelectionIndicator(relativeSelectionIdx);
                renderCategoryList(currGroupNum);

                break;

            case Page::INSTRUMENT_SELECTION:
                prevPageRelativeSelectionIdx = relativeSelectionIdx;
                activeInstrumentCategoryId = relativeSelectionIdx;
                maxRelativeSelectionIdx = INSTRUMENT_CAT_INFO_LUT[activeInstrumentCategoryId][1] - 1;
                relativeSelectionIdx = 0;
                currGroupNum = 0;

                // Render the instrument selections
                renderSelectionIndicator(relativeSelectionIdx);
                renderInstrumentList(activeInstrumentCategoryId, currGroupNum);

                break;
            
            case Page::BPM_SELECTION:
                maxRelativeSelectionIdx = -1;
                activeInstrumentCategoryId = -1;
                relativeSelectionIdx = 0;
                prevPageRelativeSelectionIdx = 0;
                
                // Render the BPM selection screen
                renderBPMSelection();

                break;
        }
    }

    void loop(int tickDelta) {
        if (tickDelta == 0) return;

        if (currPage == BPM_SELECTION) {
            currBPM += tickDelta;
            currBPM = max(MIN_BPM, min(currBPM, MAX_BPM));
            renderBPMSelection(true);
            
            return;
        }

        int newRelativeSelectionIdx = relativeSelectionIdx + tickDelta;
        newRelativeSelectionIdx = max(0, min(newRelativeSelectionIdx, maxRelativeSelectionIdx));

        if (newRelativeSelectionIdx != relativeSelectionIdx) {
            relativeSelectionIdx = newRelativeSelectionIdx;
            renderSelectionIndicator(relativeSelectionIdx);
            renderSelectedInstrumentIndicator(selectedInstrumentId, selectedInstrumentCategoryId);
        }

        int newGroupNum = relativeSelectionIdx / lcdNumRows;
        if (newGroupNum != currGroupNum) {
            currGroupNum = newGroupNum;
            lcd.clear();

            switch (currPage) {
                case INSTRUMENT_SELECTION:
                    renderInstrumentList(activeInstrumentCategoryId, currGroupNum);
                    break;
                case CATEGORY_SELECTION:
                    renderCategoryList(currGroupNum);
                    break;
            }
        }
    }

private:
    LiquidCrystal_I2C lcd;
    const int lcdNumRows;
    const int lcdNumCols;

    // Number of rows offset from the start of the top of the page
    int relativeSelectionIdx;

    // Maximum index from the top of the page the cursor can be
    int maxRelativeSelectionIdx;

    // The index of the relative selection index of the previous page
    int prevPageRelativeSelectionIdx;

    // The current group number (each group contains lcdNumRows # of items, 1 per row)
    int currGroupNum;

    int activeInstrumentCategoryId;

    // The current selected bpm
    int currBPM;

    void renderCategoryList(int currGroupNum) {
        if (NUM_CATEGORIES == 0) {
            lcd.clear();

            String errMsg = "No categories!";
            lcd.setCursor(1, 1);
            lcd.print(errMsg);

            return;
        }

        lcd.setCursor(0, 0);

        int startingIdxOffset = lcdNumRows * currGroupNum;
        int endingIdxOffset = min(NUM_CATEGORIES, startingIdxOffset + lcdNumRows);

        for (int i = startingIdxOffset; i < endingIdxOffset; ++i) {
            lcd.setCursor(1, i - startingIdxOffset);
            lcd.print(INSTRUMENT_CAT_ID_TO_NAME_LUT[i]);
        }

        renderPageNumber(currGroupNum, maxRelativeSelectionIdx);
        renderBrand();
    }

    void renderInstrumentList(int instrumentCategoryId, int currGroupNum) {
        int instrumentIdsStartingIdx = INSTRUMENT_CAT_INFO_LUT[instrumentCategoryId][0];
        int numInstrumentsInCategory = INSTRUMENT_CAT_INFO_LUT[instrumentCategoryId][1];

        if (numInstrumentsInCategory == 0) {
            lcd.clear();

            String errMsg = "No instruments!";
            lcd.setCursor(1, 1);
            lcd.print(errMsg);

            return;
        }

        int startingIdxOffset = lcdNumRows * currGroupNum;
        int endingIdxOffset = min(numInstrumentsInCategory, startingIdxOffset + lcdNumRows);

        for (int i = startingIdxOffset; i < endingIdxOffset; ++i) {
            lcd.setCursor(1, i - startingIdxOffset);
            lcd.print(INSTRUMENT_ID_TO_NAME_LUT[instrumentIdsStartingIdx + i]);
        }

        renderSelectedInstrumentIndicator(selectedInstrumentId, selectedInstrumentCategoryId);
        renderPageNumber(currGroupNum, maxRelativeSelectionIdx);
        renderBrand();
    }

    void renderBPMSelection(bool writeOnlyNewBPM = false) {
        String prefix = "BPM: ";

        if (writeOnlyNewBPM) {
            lcd.setCursor(prefix.length() + 1, 1);
            lcd.print(nSpaces(lcdNumCols - prefix.length()));

            lcd.setCursor(prefix.length() + 1, 1);
            lcd.print(String(currBPM));
        } else {
            lcd.setCursor(1, 1);
            lcd.print(String(prefix) + String(currBPM));
        }

        renderBrand();
    }

    void renderPageNumber(int currGroupNum, int maxRelativeSelectionIdx) {
        int pageNum = currGroupNum + 1;
        int maxPages = (maxRelativeSelectionIdx / lcdNumRows) + 1;

        String page_number_string = "P " + String(pageNum) + "/" + String(maxPages);
        lcd.setCursor(lcdNumCols - page_number_string.length(), 0);
        lcd.print(page_number_string);
    }

    void renderSelectedInstrumentIndicator(int selectedInstrumentId, int selectedInstrumentCategoryId) {
        if (currPage != INSTRUMENT_SELECTION) return;
        if (selectedInstrumentId == -1 || selectedInstrumentCategoryId == -1) return;
        if (selectedInstrumentCategoryId != activeInstrumentCategoryId) return;

        int numInstruments = INSTRUMENT_CAT_INFO_LUT[activeInstrumentCategoryId][1];

        if (numInstruments > 0) {
            // This is the "index" of the selected instrument in its category
            int nthInstrumentInCategory =
                getSelectedInstrumentRelativeIdx(selectedInstrumentId, selectedInstrumentCategoryId);
            int selectedRow = nthInstrumentInCategory % lcdNumRows;
            int selectedGroupNum = nthInstrumentInCategory / lcdNumRows;

            if (selectedGroupNum != currGroupNum) return;

            lcd.setCursor(0, selectedRow);
            lcd.write(SELECTED_ICON);
        }
    }

    void renderSelectionIndicator(int relativeSelectionIdx) {
        long cursorRow = relativeSelectionIdx % lcdNumRows;
        clearLCDCol(0);
        lcd.setCursor(0, cursorRow);
        lcd.write(SELECTOR_ICON);
    }

    int getSelectedInstrumentRelativeIdx(int selectedInstrumentId, int selectedInstrumentCategoryId) {
        if (selectedInstrumentId == -1 || selectedInstrumentCategoryId == -1) return -1;

        int instrumentIdsStartingIdx = INSTRUMENT_CAT_INFO_LUT[selectedInstrumentCategoryId][0];
        return selectedInstrumentId - instrumentIdsStartingIdx;
    }

    void renderBrand() {
        String brand = "DM-095";

        lcd.setCursor(lcdNumCols - brand.length(), lcdNumRows - 1);
        lcd.print(brand);
    }

    void clearLCDRow(uint8_t row) {
        if (row >= lcdNumRows) return;

        lcd.setCursor(0, row);
        const char spaces[lcdNumCols] = {" "};
        lcd.print(spaces);
    }

    void clearLCDCol(uint8_t col) {
        if (col >= lcdNumCols) return;

        for (int i = 0; i < lcdNumRows; i++) {
            lcd.setCursor(col, i);
            lcd.print(" ");
        }
    }

    String nSpaces(int n) {
        String spaces = "";
        for (int i = 0; i < n; i++) {
            spaces += " ";
        }

        return spaces;
    }
};

#endif