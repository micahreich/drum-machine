#ifndef MESSAGING_H
#define MESSAGING_H

#define MSG_START_BYTE 0xAA

enum MessageType {
    MSG_TYPE_SEQUENCE_DATA = 1,
    MSG_TYPE_PAUSE_PLAY = 2,
    MSG_TYPE_SAMPLE = 3,
    MSG_TYPE_MUTE_TRACK = 4,
    MSG_TYPE_BPM = 5,
};

#ifdef ARDUINO

#include <Arduino.h>

void sendMessage(const unsigned char *msg, size_t payload_size, const MessageType &msg_type) {
    Serial.write(static_cast<unsigned char>(MSG_START_BYTE));
    Serial.write(static_cast<unsigned char>(msg_type));
    Serial.write(payload_size);
    Serial.write(msg, payload_size);
}

void sendSequenceData(const SequenceData &data) {
    const size_t num_bytes = data.numBytes();

    unsigned char *buffer = new unsigned char[num_bytes];

    data.serialize(buffer, num_bytes);
    sendMessage(buffer, num_bytes, MSG_TYPE_SEQUENCE_DATA);

    delete[] buffer;
}

void sendPausePlay(const bool pause) {
    unsigned char msg = pause ? 1 : 0;
    sendMessage(&msg, 1, MSG_TYPE_PAUSE_PLAY);
}

void sendSample(const unsigned char instrument_id) {
    sendMessage(&instrument_id, 1, MSG_TYPE_SAMPLE);
}

void sendMuteUnmuteTrack(const unsigned char track_id) {
    sendMessage(&track_id, 1, MSG_TYPE_MUTE_TRACK);
}

void sendBPM(const unsigned char bpm) {
    sendMessage(&bpm, 1, MSG_TYPE_BPM);
}

#endif

#endif