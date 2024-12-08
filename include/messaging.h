#ifndef MESSAGING_H
#define MESSAGING_H

#define MSG_START_BYTE 0x3F

enum MessageType {
    MSG_TYPE_DRUM_SEQUENCE_DATA = 0x1,
    MSG_TYPE_PAUSE = 0x2,
    MSG_TYPE_PLAY = 0x3,
    MSG_TYPE_SAMPLE = 0x4,
    // MSG_TYPE_MUTE_TRACK = 0x5,
    // MSG_TYPE_SOLO_TRACK = 0x6,
};

#ifdef ARDUINO
#include <Arduino.h>

void sendMessage(char *msg, uint8_t payload_size, const MessageType &msg_type) {
    Serial.write(MSG_START_BYTE);
    Serial.write(msg_type);
    Serial.write(payload_size);
    Serial.write(msg, payload_size);
}

#endif

#endif