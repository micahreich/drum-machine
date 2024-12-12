#ifndef DRUM_MACHINE_STATE_H
#define DRUM_MACHINE_STATE_H

#include <Messaging.h>

struct Action {
    enum Type {
        NOOP = 0,
        TRACK_BEAT_TOGGLE = 1,
        TRACK_MUTE_TOGGLE = 2,
        PAUSE_TOGGLE = 3,
        TRACK_SELECT = 4,
        INSTRUMENT_SAMPLE = 5,
        BPM_SELECT = 6,
        CHANGE_TRACK_INSTRUMENT_ID = 7,
    };
    Type type;

    union {
        struct {
            track_id_t toggle_beat_track_id;
            unsigned char toggled_beat_id;
        };
        struct {
            track_id_t change_instrument_track_id;
            instrument_id_t new_instrument_id;
        };
        track_id_t mute_unmute_track_id;
        track_id_t new_curr_track_id;
        instrument_id_t sample_instrument_id;
        bpm_t new_bpm;
    } data;

    static Action create_TrackBeatToggle(track_id_t track_id, unsigned char toggled_beat_id) {
        Action action;
        action.type = TRACK_BEAT_TOGGLE;
        action.data.toggle_beat_track_id = track_id;
        action.data.toggled_beat_id = toggled_beat_id;

        return action;
    }

    static Action create_TrackMuteToggle(track_id_t track_id) {
        Action action;
        action.type = TRACK_MUTE_TOGGLE;
        action.data.mute_unmute_track_id = track_id;

        return action;
    }

    static Action create_PauseToggle() {
        Action action;
        action.type = PAUSE_TOGGLE;

        return action;
    }

    static Action create_TrackSelect(track_id_t track_id) {
        Action action;
        action.type = TRACK_SELECT;
        action.data.new_curr_track_id = track_id;

        return action;
    }

    static Action create_InstrumentSample(instrument_id_t instrument_id) {
        Action action;
        action.type = INSTRUMENT_SAMPLE;
        action.data.sample_instrument_id = instrument_id;

        return action;
    }

    static Action create_BPMSelect(bpm_t bpm) {
        Action action;
        action.type = BPM_SELECT;
        action.data.new_bpm = bpm;

        return action;
    }

    static Action create_ChangeTrackInstrumentID(track_id_t track_id, instrument_id_t new_instrument_id) {
        Action action;
        action.type = CHANGE_TRACK_INSTRUMENT_ID;
        action.data.change_instrument_track_id = track_id;
        action.data.new_instrument_id = new_instrument_id;

        return action;
    }

    static Action fromSerialized(MessageType &msg_type, const unsigned char *buffer) {
        switch (msg_type) {
            case MSG_TYPE_SEQUENCE_DATA: {
                track_id_t track_id = buffer[0];
                unsigned char beat_id = buffer[1];

                return create_TrackBeatToggle(track_id, beat_id);
            }

            case MSG_TYPE_MUTE_TRACK: {
                track_id_t track_id = buffer[0];

                return create_TrackMuteToggle(track_id);
            }

            case MSG_TYPE_PAUSE_PLAY: {
                return create_PauseToggle();
            }

            case MSG_TYPE_SAMPLE: {
                instrument_id_t instrument_id = buffer[0];

                return create_InstrumentSample(instrument_id);
            }

            case MSG_TYPE_BPM_SELECT: {
                bpm_t bpm = buffer[0];

                return create_BPMSelect(bpm);
            }

            case MSG_TYPE_CHANGE_TRACK_INSTRUMENT_ID: {
                track_id_t track_id = buffer[0];
                instrument_id_t instrument_id = buffer[1];

                return create_ChangeTrackInstrumentID(track_id, instrument_id);
            }

            default:
                return Action{ .type = NOOP };
        }
    }
};

#ifdef ARDUINO

#include <Arduino.h>
#include <cppQueue.h>
#include <DrumMachineTrackData.h>

#define	IMPLEMENTATION	LIFO

class DrumMachineState {
public:
    track_id_t curr_track_id;
    bool paused;

    DrumMachineState() : curr_track_id(0), paused(false) {}

    void pushAction(Action action) {
        action_queue.push(&action);
    }

    void sendActionOverSerial(Action action) {
        switch (action.type) {
            case Action::Type::TRACK_BEAT_TOGGLE: {
                unsigned char msg[2] = {action.data.toggle_beat_track_id, action.data.toggled_beat_id};
                sendMessage(msg, 2, MSG_TYPE_SEQUENCE_DATA);

                break;
            }

            case Action::Type::TRACK_MUTE_TOGGLE: {
                unsigned char msg[1] = {action.data.mute_unmute_track_id};
                sendMessage(msg, 1, MSG_TYPE_MUTE_TRACK);

                break;
            }

            case Action::Type::PAUSE_TOGGLE: {
                sendMessage(NULL, 0, MSG_TYPE_PAUSE_PLAY);

                break;
            }

            case Action::Type::INSTRUMENT_SAMPLE: {
                unsigned char msg[1] = {action.data.sample_instrument_id};
                sendMessage(msg, 1, MSG_TYPE_SAMPLE);

                break;
            }

            case Action::Type::BPM_SELECT: {
                unsigned char msg[1] = {action.data.new_bpm};
                sendMessage(msg, 1, MSG_TYPE_BPM_SELECT);

                break;
            }

            case Action::Type::CHANGE_TRACK_INSTRUMENT_ID: {
                unsigned char msg[2] = {action.data.change_instrument_track_id, action.data.new_instrument_id};
                sendMessage(msg, 2, MSG_TYPE_CHANGE_TRACK_INSTRUMENT_ID);

                break;
            }

            default:
                break;
        }
    }

    void loop() {
        // Pop all actions from the queue
        while (!action_queue.isEmpty()) {
            Action action;
            action_queue.pop(&action);
            
            switch (action.type) {
                case Action::Type::PAUSE_TOGGLE: {
                    paused = !paused;
                    break;
                }
                
                case Action::Type::TRACK_SELECT: {
                    curr_track_id = action.data.new_curr_track_id;
                    break;
                }

                default:
                    break;
            }

            sendActionOverSerial(action);
        }
    }

private:
    cppQueue action_queue = cppQueue(sizeof(Action), 10, IMPLEMENTATION);
};

#endif // DRUM_MACHINE_STATE_H
#endif // ARDUINO