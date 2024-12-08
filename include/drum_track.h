#ifndef DRUM_TRACK_H
#define DRUM_TRACK_H

#include <stdint.h>
#include <stdio.h>

#ifndef ARDUINO
#include <cstdlib>  // For malloc
#include <cstring>  // For memcpy
#endif

#define N_TRACK_SUBDIVISIONS 16

struct DrumTrackData {
    unsigned char instrument_id;
    unsigned char triggers[N_TRACK_SUBDIVISIONS];
};

struct DrumSequenceData {
    unsigned char bpm;
    unsigned char n_tracks;
    DrumTrackData *tracks;

    DrumSequenceData() {
        bpm = 0;
        n_tracks = 0;
        tracks = nullptr;
    }

    DrumSequenceData(uint8_t bpm, uint8_t n_tracks, DrumTrackData *tracks) {
        bpm = bpm;
        n_tracks = n_tracks;
        tracks = tracks;
    }

    void serialize(uint8_t *buf) {
        unsigned char idx = 0;

        buf[idx++] = bpm;
        buf[idx++] = n_tracks;

        memcpy(buf + idx, tracks, n_tracks * sizeof(DrumTrackData));
    }

    static DrumSequenceData deserialize(uint8_t *buf) {
        unsigned char idx = 0;

        unsigned char bpm = buf[idx++];
        unsigned char n_tracks = buf[idx++];
        DrumTrackData *tracks = (DrumTrackData *)malloc(n_tracks * sizeof(DrumTrackData));

        memcpy(tracks, buf + idx, n_tracks * sizeof(DrumTrackData));

        return DrumSequenceData(bpm, n_tracks, tracks);
    }

    void print() {
        printf("BPM: %d\n", bpm);
        printf("Number of tracks: %d\n", n_tracks);

        for (int i = 0; i < n_tracks; ++i) {
            printf("Track %d: ", i);

            DrumTrackData track = tracks[i];
            for (int j = 0; j < N_TRACK_SUBDIVISIONS; ++j) {
                if (track.triggers[j]) {
                    printf("X ");
                } else {
                    printf("_ ");
                }
            }
            printf("\n");
        }
    }
};

#endif