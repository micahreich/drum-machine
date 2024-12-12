#include <libserial/SerialPort.h>

using namespace LibSerial;

#include <Messaging.h>
#include <DrumMachineTrackData.h>
#include <DrumMachineState.h>
#include <AudioUtils.h>
#include <InstrumentLUT.h>
#include <unordered_map>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>
#include <queue>
#include <algorithm>
#include <boost/lockfree/spsc_queue.hpp>
#include <SFML/Audio.hpp>
#include <SwitchingSoundStream.h>

#define SERIAL_READ_TIMEOUT_MS 100

std::atomic_bool running = true;

void signalHandler(int signum) {
    std::cout << "\nInterrupt signal (" << signum << ") received. Stopping thread..." << std::endl;
    running = false;  // Set the flag to false to signal the thread to stop
}

std::vector<sf::Int16> createMixBuffer(
    const std::vector<sf::Int16>& sample_kick,
    const std::vector<sf::Int16>& sample_snare,
    LoopingStatistics& stats
) {
    int totalFrames = stats.bar_length_frames; // Example total frame count
    std::vector<sf::Int16> mix(totalFrames * AUDIO_CHANNELS, 0); // Stereo mix

    for (int i = 0; i < N_TRACK_SUBDIVISIONS; i++) {
        int frameStartIdx = i * stats.n_frames_subdivision;

        if (i == 0 || i == 4 || i == 8 || i == 12) {
            mixSample(mix, sample_kick, frameStartIdx); // Play kick on every beat
        }

        if (i == 12) {
            mixSample(mix, sample_snare, frameStartIdx); // Play kick on every beat
        }
    }

    return mix;
}

class DrumSequenceDataConsumer {
public:
    DrumSequenceDataConsumer() {
        // Initialize the audio stream
        sound_stream.setLoop(true);
        sound_stream.play();
    };

    ~DrumSequenceDataConsumer() {
        if (consumer_thread.joinable()) {
            consumer_thread.join();
        }
    }

    void spin() {
        consumer_thread = std::thread(&DrumSequenceDataConsumer::consumerThread, this);
    }
    
    boost::lockfree::spsc_queue<Action, boost::lockfree::capacity<10>> action_queue;
private:
    std::thread consumer_thread;

    // Drum sequence data, built incrementally
    SequenceData sequence_data;

    // Stores mapping from instrument ID to sample data
    std::unordered_map<instrument_id_t, AudioStreamBuffer> instrument_samples;

    // Audio buffers for each track, each with a length of 4 bars
    AudioStreamBuffer individual_tracks[N_TRACKS];

    // Looping statistics for the current sequence
    LoopingStatistics looping_stats = LoopingStatistics::fromBPM(120);

    // Instance of the sound stream which loops the current mix
    SwitchingSoundStream sound_stream = SwitchingSoundStream(AUDIO_SAMPLE_RATE, AUDIO_CHANNELS);

    void consumerThread() {
        Action action;

        while (running) {
            if (action_queue.pop(action)) {
                printf("Consumed action of type %d\n", action.type);

                switch (action.type) {
                    case Action::Type::TRACK_BEAT_TOGGLE: {
                        track_id_t track_id = action.data.toggle_beat_track_id;
                        unsigned char beat_idx = action.data.toggled_beat_id;

                        TrackData &track = sequence_data.tracks[track_id];
                        track.triggers[beat_idx] = !track.triggers[beat_idx];
                        track.n_active_triggers += track.triggers[beat_idx] ? 1 : -1;

                        sequence_data.prettyPrint();

                        if (track.triggers[beat_idx]) {
                            // Case 1: beat was toggled on, so mix in a new sample
                            addSampleToTrackByIndex(
                                individual_tracks[track_id],
                                track.instrument_id,
                                beat_idx,
                                looping_stats,
                                track.volume
                            );
                        } else {
                            // Case 2: beat was toggled off, so remove the sample
                            eraseSampleFromTrackByIndex(
                                individual_tracks[track_id],
                                track.instrument_id,
                                beat_idx,
                                looping_stats,
                                track.volume
                            );
                        }

                        // Update the sound stream with the new mix
                        AudioStreamBuffer mix_buffer = mixTracksTogether(individual_tracks, sequence_data, looping_stats);
                        sound_stream.populateIntermetideBuffer(mix_buffer, looping_stats);

                        break;
                    }

                    default:
                        break;
                }
            }
        }
    }

    void sampleInstrument(AudioStreamBuffer &buffer) {
        sf::SoundBuffer sound_buffer;
        if (!sound_buffer.loadFromSamples(buffer.data(), buffer.size(), AUDIO_CHANNELS, AUDIO_SAMPLE_RATE)) {
            std::cerr << "Failed to load audio buffer from samples!" << std::endl;
            return;
        }

        sf::Sound sample_sound;
        sample_sound.setBuffer(sound_buffer);

        // Play the sound
        sample_sound.play();

        // Keep the program running while the sound plays
        while (sample_sound.getStatus() == sf::Sound::Playing) {
            sf::sleep(sf::milliseconds(50));
        }

        return;
    }

    // Modify the track's audio buffer to remove all sound at the given beat index
    void eraseSampleFromTrackByIndex(
        AudioStreamBuffer &track,
        const AudioStreamBuffer &sample,
        unsigned char beat_idx,
        const LoopingStatistics &stats,
        float volume // This should be the volume of the sample when it was added
    ) {
        int frame_start_idx = beat_idx * stats.n_frames_subdivision;
        unmixSample(track, sample, frame_start_idx, volume);
    }

    void eraseSampleFromTrackByIndex(
        AudioStreamBuffer &track,
        instrument_id_t instrument_id,
        unsigned char beat_idx,
        const LoopingStatistics &stats,
        float volume
    ) {
        auto instrument_audio = instrument_samples.find(instrument_id);
        if (instrument_audio == instrument_samples.end()) {
            std::cerr << "Error: trying to erase instrument not found in map.\n";
        }

        const AudioStreamBuffer& sample = instrument_audio->second;
        if (sample.empty()) {
            std::cerr << "Error: samples failed to load.\n";
            return;
        }

        eraseSampleFromTrackByIndex(track, sample, beat_idx, stats, volume);
    }

    void addSampleToTrackByIndex(
        AudioStreamBuffer &track,
        const AudioStreamBuffer &sample,
        unsigned char beat_idx,
        const LoopingStatistics &stats,
        float volume
    ) {
        int frame_start_idx = beat_idx * stats.n_frames_subdivision;
        mixSample(track, sample, frame_start_idx, volume);
    }

    void addSampleToTrackByIndex(
        AudioStreamBuffer &track,
        instrument_id_t instrument_id,
        unsigned char beat_idx,
        const LoopingStatistics &stats,
        float volume = 0.75f
    ) {
        // Resize the track if it is empty
        if (track.empty()) {
            printf("Resizing track, was previously empty\n");
            int total_frames = stats.bar_length_frames;
            track.resize(total_frames * AUDIO_CHANNELS, 0);
        }

        // Load in the audio sample if we haven't already
        auto instrument_audio = instrument_samples.find(instrument_id);
        if (instrument_audio == instrument_samples.end()) {
            // If the instrument isnt yet loaded, load it into the hashmap
            const char *file_name = ABSOLUTE_PATHS[instrument_id];

            instrument_samples[instrument_id] = AudioStreamBuffer();
            int sample_rate, channels;

            loadWavFile(file_name, instrument_samples[instrument_id], sample_rate, channels);
            instrument_audio = instrument_samples.find(instrument_id);
        }

        const AudioStreamBuffer& sample = instrument_audio->second;

        if (sample.empty()) {
            std::cerr << "Error: samples failed to load.\n";
            return;
        }

        addSampleToTrackByIndex(track, sample, beat_idx, stats, volume);
    }

    AudioStreamBuffer mixTracksTogether(
        AudioStreamBuffer individual_tracks[N_TRACKS],
        SequenceData &sequence_data,
        LoopingStatistics &stats
    ) {
        int total_frames = stats.bar_length_frames;
        AudioStreamBuffer mix_buffer(total_frames * AUDIO_CHANNELS, 0);

        int n_active_tracks = 0;
        for (int j = 0; j < N_TRACKS; ++j) {
            n_active_tracks += sequence_data.tracks[j].isActive();
        }

        float volume_multiplier = 1.0f / static_cast<float>(n_active_tracks);

        for (int j = 0; j < N_TRACKS; ++j) {
            if (!sequence_data.tracks[j].isActive()) continue;

            std::transform(
                individual_tracks[j].begin(), individual_tracks[j].end(),
                mix_buffer.begin(), mix_buffer.begin(),
                [volume_multiplier](sf::Int16 v, sf::Int16 w) {
                    return static_cast<sf::Int16>(v * volume_multiplier + w);
            });
        }

        return mix_buffer;
    }
};

class DrumSequenceDataProvider {
public:
    DrumSequenceDataProvider() {
        const std::vector<std::string> ports = {
            "/dev/tty.usbmodem11301", "/dev/tty.usbmodem12301", "/dev/cu.usbmodem1101", "/dev/ttyUSB0", "/dev/ttyUSB1", "/dev/ttyACM0", "/dev/ttyACM1",
        };

        std::string connected_port = ports[0];

        for (const auto &port : ports) {
            try {
                serial.Open(port);
                serial.SetBaudRate(BaudRate::BAUD_9600);
                serial.SetCharacterSize(CharacterSize::CHAR_SIZE_8);
                serial.SetStopBits(StopBits::STOP_BITS_1);
                serial.SetParity(Parity::PARITY_NONE);

                // Clear any old data in the buffer
                serial.FlushIOBuffers();

                connected_port = port;

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
        } else {
            std::cout << "Connected to serial port " << connected_port << std::endl;
        }
    };

    ~DrumSequenceDataProvider() {
        if (serial_read_thread.joinable()) {
            serial_read_thread.join();
        }

        if (serial.IsOpen()) {
            serial.Close();
        }
    }

    void spin() {
        serial_read_thread = std::thread(&DrumSequenceDataProvider::serialReadThread, this);
    }

    void attachDataConsumer(DrumSequenceDataConsumer *consumer) {
        data_consumer = consumer;
    }

private:
    SerialPort serial;
    std::thread serial_read_thread;
    DrumSequenceDataConsumer *data_consumer = nullptr;

    void serialReadThread() {
        while (running) {
            readAndDecodeMessage();
        }
    }

    void readAndDecodeMessage() {
        if (!serial.IsDataAvailable()) return;

        uint8_t start_byte;
        serial.ReadByte(start_byte, SERIAL_READ_TIMEOUT_MS);

        if (start_byte != MSG_START_BYTE) return;  // Ignore if not a valid start byte

        // Read message header
        uint8_t msg_type;
        uint8_t payload_size;
        DataBuffer payload;

        serial.ReadByte(msg_type, SERIAL_READ_TIMEOUT_MS);
        serial.ReadByte(payload_size, SERIAL_READ_TIMEOUT_MS);

        if (payload_size > 0) {
            serial.Read(payload, payload_size, SERIAL_READ_TIMEOUT_MS);
        }

        const unsigned char *buffer = static_cast<const unsigned char *>(payload.data());

        MessageType variant = static_cast<MessageType>(msg_type);
        Action action = Action::fromSerialized(variant, buffer);

        printf("Received message of type %d (with payload size %d bytes)\n", msg_type, payload_size);

        if (data_consumer != nullptr) {
            while (running && !data_consumer->action_queue.push(action));
        }
    }
};


int main() {
    std::signal(SIGINT, signalHandler);

    DrumSequenceDataConsumer data_consumer = DrumSequenceDataConsumer();
    DrumSequenceDataProvider data_provider = DrumSequenceDataProvider();
    data_provider.attachDataConsumer(&data_consumer);

    data_consumer.spin();
    data_provider.spin();

    return 0;
}