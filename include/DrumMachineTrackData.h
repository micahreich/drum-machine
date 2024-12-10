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

struct TrackData {
    bool muted;
    unsigned char instrument_id;
    unsigned char triggers[N_TRACK_SUBDIVISIONS];

    bool isActive() const {
        for (unsigned char i = 0; i < N_TRACK_SUBDIVISIONS; i++) {
            if (triggers[i] != 0) {
                return true;
            }
        }
        return false;
    }
};

struct SequenceData {
    unsigned char bpm;
    TrackData tracks[N_TRACKS];

    size_t numBytes() const {
        return sizeof(unsigned char) + N_TRACKS * sizeof(TrackData);
    }

    size_t serialize(unsigned char *buffer, size_t buffer_size) const {
        size_t offset = 0;

        // Serialize bpm
        buffer[offset++] = bpm;

        // Serialize each TrackData
        for (unsigned char i = 0; i < N_TRACKS; i++) {
            // Serialize instrument_id
            buffer[offset++] = tracks[i].instrument_id;

            // Serialize triggers
            memcpy(buffer + offset, tracks[i].triggers, sizeof(tracks[i].triggers));
            offset += sizeof(tracks[i].triggers);
        }

        return offset; // Return total serialized size
    }

    SequenceData deserializeSequenceData(const unsigned char *buffer, size_t buffer_size) {
        size_t offset = 0;
        SequenceData data;

        // Deserialize bpm
        data.bpm = buffer[offset++];

        // Deserialize each TrackData
        for (unsigned char i = 0; i < N_TRACKS; i++) {
            // Deserialize instrument_id
            data.tracks[i].instrument_id = buffer[offset++];

            // Deserialize triggers
            memcpy(data.tracks[i].triggers, buffer + offset, sizeof(data.tracks[i].triggers));
            offset += sizeof(data.tracks[i].triggers);
        }

        return data;
    }
};

#endif