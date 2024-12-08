#include <drum_track.h>
#include <libserial/SerialPort.h>

using namespace LibSerial;

#include <instrument_lut.h>
#include <messaging.h>

#include <SFML/Audio.hpp>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#define SERIAL_READ_TIMEOUT_MS 100

typedef unsigned char instrument_id;

constexpr const char *ARDUINO_SERIAL_PORT = "/dev/ttyUSB0";
std::atomic_bool running = true;

void signalHandler(int signum) {
    std::cout << "\nInterrupt signal (" << signum << ") received. Stopping thread..." << std::endl;
    running = false;  // Set the flag to false to signal the thread to stop
}

double secondsPerSubdivision(uint8_t bpm) {
    double beats_per_second = bpm / 60.0;
    double seconds_per_beat = 1.0 / beats_per_second;
    double seconds_per_bar = seconds_per_beat * 4;
    double seconds_per_subdivision = seconds_per_bar / N_TRACK_SUBDIVISIONS;

    return seconds_per_subdivision;
}

struct InstrumentSequence {
    std::vector<std::vector<instrument_id>> instrument_sequence;
    double seconds_per_subdivision;

    // Constructors

    InstrumentSequence() {
        instrument_sequence = std::vector<std::vector<instrument_id>>(N_TRACK_SUBDIVISIONS);
        seconds_per_subdivision = 0.0;
    }

    InstrumentSequence(std::vector<std::vector<instrument_id>> &&seq, double secs)
        : instrument_sequence(std::move(seq)), seconds_per_subdivision(secs) {}

    InstrumentSequence(const DrumSequenceData &drum_seq) {
        // Populate the instrument sequence based on which instruments are
        // triggered on each 16th node
        std::vector<std::vector<instrument_id>> parsed_instrument_sequence =
            std::vector<std::vector<instrument_id>>(N_TRACK_SUBDIVISIONS);

        for (int i = 0; i < drum_seq.n_tracks; ++i) {
            DrumTrackData track = drum_seq.tracks[i];
            for (int j = 0; j < N_TRACK_SUBDIVISIONS; ++j) {
                if (track.triggers[j]) {
                    parsed_instrument_sequence[j].push_back(track.instrument_id);
                }
            }
        }

        instrument_sequence = std::move(parsed_instrument_sequence);
        seconds_per_subdivision = secondsPerSubdivision(drum_seq.bpm);
    }
};

class DrumSequenceDataProvider {
public:
    using Callback = std::function<void()>;
    enum CallbackType {
        DRUM_SEQUENCE_DATA,
        SAMPLE_INSTRUMENT,
        PAUSE,
        PLAY,
    };

    DrumSequenceDataProvider(bool read_serial) : drum_seq(DrumSequenceData()) {
        if (read_serial) {
            const std::vector<std::string> ports = {
                "/dev/tty.usbmodem12301", "/dev/ttyUSB0", "/dev/ttyUSB1", "/dev/ttyACM0", "/dev/ttyACM1",
            };

            for (const auto &port : ports) {
                try {
                    serial.Open(port);
                    serial.SetBaudRate(BaudRate::BAUD_9600);
                    serial.SetCharacterSize(CharacterSize::CHAR_SIZE_8);
                    serial.SetStopBits(StopBits::STOP_BITS_1);
                    serial.SetParity(Parity::PARITY_NONE);

                    // Clear any old data in the buffer
                    serial.FlushIOBuffers();

                    // Wait briefly for the Arduino to reset (typical on connection)
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));

                    break;
                } catch (const OpenFailed &) {
                    std::cerr << "Failed to open port " << port << std::endl;
                } catch (...) {
                    std::cerr << "An error occurred with port " << port << std::endl;
                }
            }

            if (!serial.IsOpen()) {
                std::cerr << "Failed to open any serial port!" << std::endl;
                exit(1);
            }
        }
    };
    DrumSequenceDataProvider(const DrumSequenceData &data) : drum_seq(data) {};

    ~DrumSequenceDataProvider() {
        serial_read_thread.join();

        if (serial.IsOpen()) {
            serial.Close();
        }
    }

    void registerCallback(CallbackType type, Callback callback) {
        switch (type) {
            case DRUM_SEQUENCE_DATA:
                drum_sequence_data_callback = callback;
                break;
            case SAMPLE_INSTRUMENT:
                sample_instrument_callback = callback;
                break;
            case PAUSE:
                pause_callback = callback;
                break;
            case PLAY:
                play_callback = callback;
                break;
        }
    }

    const DrumSequenceData &getDrumSequence() { return drum_seq; }
    const instrument_id getSampleInstrumentId() { return sample_instrument_id; }

    void setDrumSequence(const DrumSequenceData &data) {
        drum_seq = data;

        if (drum_sequence_data_callback) {
            std::cout << "Calling subscriber callback..." << std::endl;
            drum_sequence_data_callback();
        }
    }

    void spin() { serial_read_thread = std::thread(&DrumSequenceDataProvider::serialReadThread, this); }

private:
    SerialPort serial;
    std::thread serial_read_thread;

    DrumSequenceData drum_seq;
    instrument_id sample_instrument_id;

    Callback drum_sequence_data_callback;
    Callback sample_instrument_callback;
    Callback pause_callback;
    Callback play_callback;

    void serialReadThread() {
        while (1) {
            if (!running) return;
            readAndDecodeMessage();
        }
    }

    void readAndDecodeMessage() {
        if (!serial.IsDataAvailable()) return;

        uint8_t start_byte;
        serial.ReadByte(start_byte, SERIAL_READ_TIMEOUT_MS);

        if (start_byte != MSG_START_BYTE) return;  // Ignore if not a valid start byte

        // Read message header
        uint8_t msg_type, payload_size;
        serial.ReadByte(msg_type, SERIAL_READ_TIMEOUT_MS);
        serial.ReadByte(payload_size, SERIAL_READ_TIMEOUT_MS);

        DataBuffer payload;
        MessageType variant = static_cast<MessageType>(msg_type);

        switch (variant) {
            case MSG_TYPE_DRUM_SEQUENCE_DATA:
                std::cout << "Received MSG_TYPE_DRUM_SEQUENCE_DATA message!" << std::endl;

                // Read the DrumSequenceData payload
                serial.Read(payload, payload_size, SERIAL_READ_TIMEOUT_MS);

                // Deserialize the DrumSequenceData
                drum_seq = DrumSequenceData::deserialize(payload.data());

                // if (subscriber_callback) {
                //     std::cout << "Calling subscriber callback..." << std::endl;
                //     subscriber_callback();
                // }

                break;
            case MSG_TYPE_SAMPLE:
                std::cout << "Received MSG_TYPE_SAMPLE message!" << std::endl;

                serial.Read(payload, payload_size, SERIAL_READ_TIMEOUT_MS);
                sample_instrument_id = reinterpret_cast<instrument_id>(payload[0]);

                if (sample_instrument_callback) {
                    std::cout << "Calling sample instrument callback..." << std::endl;
                    sample_instrument_callback();
                }

                break;
            case MSG_TYPE_PAUSE:
                std::cout << "Received MSG_TYPE_PAUSE message!" << std::endl;
                break;
            case MSG_TYPE_PLAY:
                std::cout << "Received MSG_TYPE_PLAY message!" << std::endl;
                break;
        }
    }
};

enum DrumSequenceMixerState {
    LOOP = 0,
    PAUSED = 1,
    SAMPLE = 2,
};

class DrumSequenceMixer {
public:
    DrumSequenceMixer(DrumSequenceDataProvider &data_provider)
        : data_provider(data_provider),
          beat_num(0),
          beat_idx(0),
          beat_reset(true),
          beat_start_time(std::chrono::steady_clock::now()),
          state(PAUSED) {
        data_provider.registerCallback(DrumSequenceDataProvider::CallbackType::DRUM_SEQUENCE_DATA,
                                       [this]() { drumSequenceDataCallback(); });

        data_provider.registerCallback(DrumSequenceDataProvider::CallbackType::SAMPLE_INSTRUMENT,
                                       [this]() { sampleInstrumentCallback(); });

        // Load in sounds from the lookup table
        instrument_buffers = std::vector<sf::SoundBuffer>(NUM_INSTRUMENTS);
        instrument_sounds = std::vector<sf::Sound>(NUM_INSTRUMENTS);

        for (size_t i = 0; i < NUM_INSTRUMENTS; ++i) {
            if (!instrument_buffers[i].loadFromFile(ABSOLUTE_PATHS[i])) {
                std::cerr << "Error loading sound files!" << std::endl;
                return;
            }

            instrument_sounds[i].setBuffer(instrument_buffers[i]);
        }

        for (sf::Sound &sound : instrument_sounds) {
            sound.play();
            while (sound.getStatus() == sf::Sound::Playing) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }

    ~DrumSequenceMixer() {
        if (drum_mixer_thread.joinable()) {
            drum_mixer_thread.join();
        }
    }

    void spin() { drum_mixer_thread = std::thread(&DrumSequenceMixer::drumMixerThread, this); }

    void pauseCallback() {
        state_mutex.lock();

        state = PAUSED;

        state_mutex.unlock();
    }

    void playCallback() {
        state_mutex.lock();

        state = LOOP;
        beat_reset = true;

        state_mutex.unlock();
    }

private:
    std::thread drum_mixer_thread;

    DrumSequenceDataProvider &data_provider;

    std::mutex state_mutex;
    InstrumentSequence curr_instrument_seq;

    unsigned int beat_num;
    bool beat_reset;
    size_t beat_idx;

    unsigned int curr_sample_instrument_num;
    std::chrono::time_point<std::chrono::steady_clock> beat_start_time;

    DrumSequenceMixerState state;

    // Load in sounds from files
    std::vector<sf::SoundBuffer> instrument_buffers;
    std::vector<sf::Sound> instrument_sounds;

    void drumSequenceDataCallback() {
        std::cout << "DrumSequenceData callback called! Waiting for lock..." << std::endl;

        state_mutex.lock();

        // Extract the InstrumentSequence from the new DrumSequenceData and
        // reset the beat counter
        const DrumSequenceData &new_drum_seq = data_provider.getDrumSequence();

        if (new_drum_seq.bpm == 0) {
            state_mutex.unlock();

            std::cout << "Error: new drum sequence data has 0 bpm!" << std::endl;
            return;
        } else {
            std::cout << "New DrumSequenceData received!" << std::endl;
        }

        curr_instrument_seq = InstrumentSequence(new_drum_seq);
        state = LOOP;
        beat_reset = true;
        beat_idx = 0;

        state_mutex.unlock();
    }

    void sampleInstrumentCallback() {
        std::cout << "Sample instrument callback called! Waiting for lock..." << std::endl;

        state_mutex.lock();

        curr_sample_instrument_num = data_provider.getSampleInstrumentId();
        state = SAMPLE;

        state_mutex.unlock();
    }

    void drumMixerThread() {
        using namespace std::chrono;
        steady_clock::time_point next_call_time;
        const auto DO_NOTHING_INTERVAL = milliseconds(50);

        DrumSequenceMixerState prev_state = state;
        DrumSequenceMixerState curr_state = prev_state;

        while (1) {
            if (!running) return;

            state_mutex.lock();

            prev_state = curr_state;
            curr_state = state;

            if (prev_state != curr_state) {
                std::cout << "State transition: " << prev_state << " -> " << curr_state << std::endl;
            }

            switch (state) {
                case LOOP:
                    // Lock the mutex before reading the shared variable

                    if (!beat_reset) {
                        while (steady_clock::now() < next_call_time);
                    } else {
                        beat_start_time = std::chrono::steady_clock::now();
                        beat_reset = false;
                        beat_num = 0;
                    }

                    for (instrument_id id : curr_instrument_seq.instrument_sequence[beat_idx]) {
                        instrument_sounds[id].play();
                    }

                    beat_idx = (beat_idx + 1) % N_TRACK_SUBDIVISIONS;

                    state_mutex.unlock();

                    // Calculate the next time to trigger the instruments
                    ++beat_num;
                    next_call_time = beat_start_time + duration_cast<steady_clock::duration>(duration<double>(
                                                           curr_instrument_seq.seconds_per_subdivision * beat_num));

                    std::this_thread::sleep_until(next_call_time -
                                                  duration<double>(0.2 * curr_instrument_seq.seconds_per_subdivision));

                    break;
                case PAUSED:
                    state_mutex.unlock();

                    std::this_thread::sleep_for(DO_NOTHING_INTERVAL);

                    break;
                case SAMPLE:
                    instrument_sounds[curr_sample_instrument_num].play();

                    while (instrument_sounds[curr_sample_instrument_num].getStatus() == sf::Sound::Playing) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(DO_NOTHING_INTERVAL));
                    }

                    state_mutex.unlock();
                    state = PAUSED;

                    break;
            }
        }
    }
};

int main() {
    std::signal(SIGINT, signalHandler);

    // // Load sound buffers
    // sf::SoundBuffer buffer1, buffer2;
    // if (!buffer1.loadFromFile("kick.wav") ||
    // !buffer2.loadFromFile("snare.wav")) {
    //     std::cerr << "Error loading sound files!" << std::endl;
    //     return -1;
    // }

    // // Create sound objects and attach the buffers
    // sf::Sound sound1, sound2;
    // sound1.setBuffer(buffer1);
    // sound2.setBuffer(buffer2);

    // // Play the sounds on separate channels simultaneously
    // sound1.play();
    // sound2.play();

    // // Optionally adjust volume and pitch for each sound
    // sound1.setVolume(75); // Adjust volume to 75% for sound1
    // sound2.setVolume(50); // Adjust volume to 50% for sound2
    // sound1.setPitch(1.0f); // Normal pitch for sound1
    // sound2.setPitch(1.2f); // Slightly higher pitch for sound2

    // // Keep the program running while sounds are playing
    // while (sound1.getStatus() == sf::Sound::Playing || sound2.getStatus() ==
    // sf::Sound::Playing) {
    //     std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // }

    // // Sample beat
    // const bool silent[N_TRACK_SUBDIVISIONS] = {0};

    // DrumTrackData kick_track = DrumTrackData{0, {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0}};
    // DrumTrackData clap_track = DrumTrackData{1, {0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0}};
    // DrumTrackData hihat_track = DrumTrackData{2, {0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0}};

    // // DrumTrackData tracks[] = {kick_track, clap_track, hihat_track};

    // // DrumSequenceData sample = DrumSequenceData{150, 3, tracks};

    // DrumSequenceDataProvider data_provider = DrumSequenceDataProvider(true);
    // DrumSequenceMixer mixer = DrumSequenceMixer(data_provider);

    // // Drum sequence 1
    // DrumTrackData tracks_1[] = {kick_track};
    // DrumSequenceData sample_1 = DrumSequenceData{150, 1, tracks_1};

    // // Drum sequence 2
    // DrumTrackData tracks_2[] = {kick_track, clap_track};
    // DrumSequenceData sample_2 = DrumSequenceData{150, 2, tracks_2};

    // // Drum sequence 3
    // DrumTrackData tracks_3[] = {kick_track, clap_track, hihat_track};
    // DrumSequenceData sample_3 = DrumSequenceData{150, 3, tracks_3};

    // mixer.spin();

    // data_provider.setDrumSequence(sample_1);
    // std::this_thread::sleep_for(std::chrono::milliseconds(3200));

    // data_provider.setDrumSequence(sample_2);
    // std::this_thread::sleep_for(std::chrono::milliseconds(3200));

    // data_provider.setDrumSequence(sample_3);
    // std::this_thread::sleep_for(std::chrono::milliseconds(3200));

    // mixer.pauseCallback();
    // std::this_thread::sleep_for(std::chrono::milliseconds(600));
    // mixer.playCallback();

    DrumSequenceDataProvider data_provider = DrumSequenceDataProvider(true);
    DrumSequenceMixer mixer = DrumSequenceMixer(data_provider);

    data_provider.spin();
    mixer.spin();

    return 0;
}