#ifdef ARDUINO
#ifndef DRUM_MACHINE_STATE_H
#define DRUM_MACHINE_STATE_H

#include <Arduino.h>
#include <cppQueue.h>
#include <DrumMachineTrackData.h>
#include <Messaging.h>

#define	IMPLEMENTATION	LIFO

struct Action {
    enum Type { TRACK_BEAT_TOGGLE, TRACK_MUTE_TOGGLE, PAUSE_TOGGLE, TRACK_SELECT, INSTRUMENT_SAMPLE, BPM_SELECT };
    Type type;

    union {
        struct {
            int track_id;
            int beat_id;
        };
        int instrument_id;
        int bpm;
    } data;
};

class DrumMachineState {
public:
    int curr_track_id;

    DrumMachineState() : curr_track_id(0), sequence_data(SequenceData()), paused(false) {}

    void pushAction(Action action) {
        action_queue.push(&action);
    }

    void loop() {
        // Pop all actions from the queue
        while (!action_queue.isEmpty()) {
            Action action;
            action_queue.pop(&action);

            switch (action.type) {
                case Action::Type::TRACK_BEAT_TOGGLE: {
                    bool curr_beat_state = sequence_data.tracks[curr_track_id].triggers[action.data.beat_id];
                    sequence_data.tracks[curr_track_id].triggers[action.data.beat_id] = !curr_beat_state;

                    // Serialize and send the sequence data over serial
                    sendSequenceData(sequence_data);

                    break;
                }

                case Action::Type::TRACK_MUTE_TOGGLE: {
                    bool curr_mute_state = sequence_data.tracks[action.data.track_id].muted;
                    sequence_data.tracks[action.data.track_id].muted = !curr_mute_state;
                    
                    // Serialize and send the action over serial
                    sendMuteUnmuteTrack(action.data.track_id);

                    break;
                }
                
                case Action::Type::PAUSE_TOGGLE: {
                    paused = !paused;

                    // Serialize and send the action over serial
                    sendPausePlay(paused);

                    break;
                }
                
                case Action::Type::TRACK_SELECT: {
                    curr_track_id = action.data.track_id;

                    break;
                }

                case Action::Type::INSTRUMENT_SAMPLE: {
                    int instrument_id = action.data.instrument_id;

                    // Serialize and send the action over serial
                    sendSample(instrument_id);

                    break;
                }

                case Action::Type::BPM_SELECT: {
                    int bpm = action.data.bpm;
                    sequence_data.bpm = bpm;

                    // Serialize and send the sequence data over serial
                    sendBPM(bpm);

                    break;
                }
            }
        }
    }

private:
    cppQueue action_queue = cppQueue(sizeof(Action), 10, IMPLEMENTATION);
    SequenceData sequence_data;
    bool paused;
};

#endif // DRUM_MACHINE_STATE_H
#endif // ARDUINO