#ifndef DRUM_MACHINE_BUTTONS_H
#define DRUM_MACHINE_BUTTONS_H

#include <Arduino.h>
#include <Button2.h>
#include <Encoder.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <ShiftRegisterDebounce.h>
#include <DrumMachineTrackData.h>
#include <cppQueue.h>

#define BUTTON_DEBOUNCE_MS 25

typedef void (*BeatButtonRiseCallback)(int);  // Beat index
typedef void (*TrackButtonRiseCallback)(int); // Track index
typedef void (*DrumMachineButtonEventCallback)();

class DrumMachineButtons {
public:
    DrumMachineButtons(
        uint8_t *shift_register_data
    ) :
        shift_register_data(shift_register_data),
        beat_button_rise_callback(NULL),
        track_button_rise_callback(NULL),
        pause_play_button_rise_callback(NULL),
        mute_unmute_button_rise_callback(NULL),
        sample_button_rise_callback(NULL) {

        if (shift_register_data == NULL) {
            Serial.println("Shift register data is NULL!");
            return;
        }

        for (int i = 0; i < N_TRACK_SUBDIVISIONS; ++i) {
            beat_buttons[i].setButtonIndex(i);
            beat_buttons[i].setShiftRegisterData(shift_register_data);
        }

        for (int i = 0; i < N_TRACKS; ++i) {
            track_buttons[i].setButtonIndex(N_TRACK_SUBDIVISIONS + 3 + i);
            track_buttons[i].setShiftRegisterData(shift_register_data);
        }
    }

    void setup() {
        for (int i = 0; i < N_TRACK_SUBDIVISIONS; ++i) {
            beat_buttons[i].interval(BUTTON_DEBOUNCE_MS);
        }

        for (int i = 0; i < N_TRACKS; ++i) {
            track_buttons[i].interval(BUTTON_DEBOUNCE_MS);
        }

        pause_play_button.interval(BUTTON_DEBOUNCE_MS);
        mute_unmute_button.interval(BUTTON_DEBOUNCE_MS);
        sample_button.interval(BUTTON_DEBOUNCE_MS);
    }

    void loop() {
        for (int i = 0; i < N_TRACK_SUBDIVISIONS; ++i) {
            beat_buttons[i].update();

            if (beat_buttons[i].rose()) {
                if (beat_button_rise_callback != NULL) {
                    beat_button_rise_callback(i);
                }
            }
        }

        for (int i = 0; i < N_TRACKS; ++i) {
            track_buttons[i].update();

            if (track_buttons[i].rose()) {
                if (track_button_rise_callback != NULL) {
                    track_button_rise_callback(i);
                }
            }
        }

        pause_play_button.update();
        if (pause_play_button.rose()) {
            if (pause_play_button_rise_callback != NULL) {
                pause_play_button_rise_callback();
            }
        }

        mute_unmute_button.update();
        if (mute_unmute_button.rose()) {
            if (mute_unmute_button_rise_callback != NULL) {
                mute_unmute_button_rise_callback();
            }
        }

        sample_button.update();
        if (sample_button.rose()) {
            if (sample_button_rise_callback != NULL) {
                sample_button_rise_callback();
            }
        }
    }

    void setBeatButtonRiseHandler(BeatButtonRiseCallback callback) {
        beat_button_rise_callback = callback;
    }

    void setTrackButtonRiseHandler(TrackButtonRiseCallback callback) {
        track_button_rise_callback = callback;
    }

    void setPausePlayButtonRiseHandler(DrumMachineButtonEventCallback callback) {
        pause_play_button_rise_callback = callback;
    }

    void setMuteUnmuteButtonRiseHandler(DrumMachineButtonEventCallback callback) {
        mute_unmute_button_rise_callback = callback;
    }

    void setSampleButtonRiseHandler(DrumMachineButtonEventCallback callback) {
        sample_button_rise_callback = callback;
    }
private:
    // 3x Shift Registers
    uint8_t *shift_register_data = NULL;
    ShiftRegisterBounce beat_buttons[N_TRACK_SUBDIVISIONS];
    ShiftRegisterBounce track_buttons[N_TRACKS];

    ShiftRegisterBounce pause_play_button = ShiftRegisterBounce(shift_register_data, N_TRACK_SUBDIVISIONS);
    ShiftRegisterBounce mute_unmute_button = ShiftRegisterBounce(shift_register_data, N_TRACK_SUBDIVISIONS + 1);
    ShiftRegisterBounce sample_button = ShiftRegisterBounce(shift_register_data, N_TRACK_SUBDIVISIONS + 2);

    // Callback functions
    BeatButtonRiseCallback beat_button_rise_callback = NULL;
    TrackButtonRiseCallback track_button_rise_callback = NULL;
    DrumMachineButtonEventCallback pause_play_button_rise_callback = NULL;
    DrumMachineButtonEventCallback mute_unmute_button_rise_callback = NULL;
    DrumMachineButtonEventCallback sample_button_rise_callback = NULL;
};

#endif // DRUM_MACHINE_BUTTONS_H