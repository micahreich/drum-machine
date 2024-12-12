#ifndef DRUM_TRACK_H
#define DRUM_TRACK_H

#include <stdint.h>
#include <stdio.h>

#ifndef ARDUINO
#include <cstdlib>  // For malloc
#include <cstring>  // For memcpy
#endif

#define N_TRACK_SUBDIVISIONS 16
#define N_TRACKS 5

typedef unsigned char instrument_id_t;
typedef unsigned char bpm_t;
typedef unsigned char track_id_t;

struct TrackData {
    bool muted;
    float volume;
    instrument_id_t instrument_id;
    unsigned char n_active_triggers;
    bool triggers[N_TRACK_SUBDIVISIONS];

    TrackData() : muted(false), volume(0.75f), instrument_id(0), n_active_triggers(0) {}

    bool isActive() const {
        return n_active_triggers > 0 && !muted;
    }
};

struct SequenceData {
    bpm_t bpm;
    TrackData tracks[N_TRACKS];

    SequenceData() : bpm(120) {}

    void reset() {
        bpm = 120;
        for (unsigned char i = 0; i < N_TRACKS; i++) {
            tracks[i].instrument_id = 0;
            tracks[i].volume = 0.75f;
            tracks[i].muted = false;
            tracks[i].n_active_triggers = 0;

            for (unsigned char j = 0; j < N_TRACK_SUBDIVISIONS; j++) {
                tracks[i].triggers[j] = false;
            }
        }
    }

    void prettyPrint() const {
        printf("BPM: %d\n", bpm);

        for (unsigned char i = 0; i < N_TRACKS; i++) {
            printf(
                "Track %d: (instrument=%d, vol=%f, muted: %d)\ttriggers=[",
                i, tracks[i].instrument_id, tracks[i].volume, tracks[i].muted
            );

            for (unsigned char j = 0; j < N_TRACK_SUBDIVISIONS; j++) {
                printf("%d", tracks[i].triggers[j]);
                if (j < N_TRACK_SUBDIVISIONS - 1) {
                    printf(", ");
                }
            }
            printf("]\n");
        }
    }

    // #ifndef ARDUINO
    // void prettyPrint() const {
    //     printf("BPM: %d\n", bpm);

    //     for (unsigned char i = 0; i < N_TRACKS; i++) {
    //         printf("Track %d: instrument_id=%d, triggers=[", i, tracks[i].instrument_id);
    //         for (unsigned char j = 0; j < N_TRACK_SUBDIVISIONS; j++) {
    //             printf("%d", tracks[i].triggers[j]);
    //             if (j < N_TRACK_SUBDIVISIONS - 1) {
    //                 printf(", ");
    //             }
    //         }
    //         printf("]\n");
    //     }
    // }
    // #endif

    // size_t numBytes() const {
    //     return sizeof(unsigned char) + N_TRACKS * sizeof(TrackData);
    // }

    // size_t serialize(unsigned char *buffer, size_t buffer_size) const {
    //     size_t offset = 0;

    //     // Serialize bpm
    //     buffer[offset++] = bpm;

    //     // Serialize each TrackData
    //     for (unsigned char i = 0; i < N_TRACKS; i++) {
    //         // Serialize instrument_id
    //         buffer[offset++] = tracks[i].instrument_id;

    //         // Serialize triggers
    //         memcpy(buffer + offset, tracks[i].triggers, sizeof(tracks[i].triggers));
    //         offset += sizeof(tracks[i].triggers);
    //     }

    //     return offset; // Return total serialized size
    // }

    // static SequenceData deserialize(const unsigned char *buffer) {
    //     size_t offset = 0;
    //     SequenceData data;

    //     // Deserialize bpm
    //     data.bpm = buffer[offset++];

    //     // Deserialize each TrackData
    //     for (unsigned char i = 0; i < N_TRACKS; i++) {
    //         // Deserialize instrument_id
    //         data.tracks[i].instrument_id = buffer[offset++];

    //         // Deserialize triggers
    //         memcpy(data.tracks[i].triggers, buffer + offset, sizeof(data.tracks[i].triggers));
    //         offset += sizeof(data.tracks[i].triggers);
    //     }

    //     return data;
    // }
};

#endif