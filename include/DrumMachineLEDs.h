#ifndef DRUM_MACHINE_LEDS_H
#define DRUM_MACHINE_LEDS_H

#ifdef ARDUINO

#include <Adafruit_NeoPixel.h>
#include <DrumMachineTrackData.h>

class DrumMachineLEDs {
public:
    DrumMachineLEDs(Adafruit_NeoPixel strip_beat, Adafruit_NeoPixel strip_track) : strip_beat(strip_beat), strip_track(strip_track) {
        ACTIVE_BEAT_IDX_COLOR = strip_beat.Color(0, 128, 255);

        INACTIVE_BEAT_IDX_COLORS[0] = strip_beat.Color(255, 0, 0);
        INACTIVE_BEAT_IDX_COLORS[1] = strip_beat.Color(255, 165, 0);
        INACTIVE_BEAT_IDX_COLORS[2] = strip_beat.Color(255, 100, 0);
        INACTIVE_BEAT_IDX_COLORS[3] = strip_beat.Color(255, 50, 0);

        ACTIVE_TRACK_IDX_COLOR = strip_track.Color(0, 128, 255);
    }

    void init() {
        strip_beat.begin(); // Prepares the data pin NeoPixels are connected to
        strip_beat.setBrightness(128); // max brightness is 255
        strip_beat.show();  // Initialize all the pixels to 'off' since nothing has been programmed yet

        strip_track.begin(); // Prepares the data pin NeoPixels are connected to
        strip_track.setBrightness(128); // max brightness is 255
        strip_track.show();  // Initialize all the pixels to 'off' since nothing has been programmed yet
    }

    void loop(const SequenceData &sequence_data, track_id_t active_track_id) {
        strip_beat.clear();
        
        // Light up the active track
        TrackData track = sequence_data.tracks[active_track_id];

        for (int j = 0; j < N_TRACK_SUBDIVISIONS; ++j) {
            if (track.triggers[j]) {
                strip_beat.setPixelColor(j, ACTIVE_BEAT_IDX_COLOR);
            } else {
                strip_beat.setPixelColor(j, INACTIVE_BEAT_IDX_COLORS[j % 4]);
            }
        }

        strip_beat.show();

        // Light up the active track
        strip_track.clear();
        strip_track.setPixelColor(active_track_id, ACTIVE_TRACK_IDX_COLOR);

        strip_track.show();
    }


private:
    Adafruit_NeoPixel strip_beat;
    Adafruit_NeoPixel strip_track;

    uint32_t ACTIVE_BEAT_IDX_COLOR;
    uint32_t INACTIVE_BEAT_IDX_COLORS[4];

    uint32_t ACTIVE_TRACK_IDX_COLOR;
};

#endif
#endif