#include <SFML/Audio.hpp>
#include <iostream>
#include <vector>
#include <algorithm>
#include <thread>
#include <chrono>
#include <regex>
#include <mutex>
#include <DrumMachineTrackData.h>

#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_CHANNELS 2

typedef std::vector<sf::Int16> AudioStreamBuffer;


struct LoopingStatistics {
    bpm_t bpm;
    int beat_interval_frames;
    int bar_length_frames;
    int n_frames_subdivision;

    static LoopingStatistics fromBPM(bpm_t bpm) {
        LoopingStatistics stats;
        stats.bpm = bpm;
        stats.beat_interval_frames = AUDIO_SAMPLE_RATE * 60 / bpm;
        stats.bar_length_frames = stats.beat_interval_frames * 4;
        stats.n_frames_subdivision = stats.bar_length_frames / N_TRACK_SUBDIVISIONS;

        return stats;
    }
};


class SwitchingSoundStream : public sf::SoundStream {
public:
    SwitchingSoundStream(int sampleRate, int channels) :
        m_sampleRate(sampleRate),
        m_channels(channels),
        curr_buffer_index(0),
        dirty(false) {
        // Initialize the stream
        initialize(m_channels, m_sampleRate);

        // Create the initial (silent) mix buffer
        looping_stats_A = LoopingStatistics::fromBPM(120);
        int total_frames = looping_stats_A.bar_length_frames;
        mix_buffer_A.resize(total_frames * m_channels, 0);

        curr_mix_buffer = &mix_buffer_A;
        curr_looping_stats = &looping_stats_A;
    }

    void populateIntermetideBuffer(const AudioStreamBuffer& mix, const LoopingStatistics& stats) {
        std::lock_guard<std::mutex> lock(mtx);

        if (curr_buffer_index == 0) {
            mix_buffer_B = std::move(mix);
            looping_stats_B = stats;
        } else {
            mix_buffer_A = std::move(mix);
            looping_stats_A = stats;
        }

        dirty = true;
    }

protected:
    virtual bool onGetData(Chunk& data) override {
        // Provide the next chunk of samples
        std::lock_guard<std::mutex> lock(mtx);

        if (dirty) {
            printf("Noticed dirty buffer, swapping...\n");
            swapIntermediateIntoCurrentBuffer();
            CHUNK_SIZE = curr_looping_stats->n_frames_subdivision;
        }

        if (m_playbackPosition < curr_mix_buffer->size()) {
            data.samples = &(*curr_mix_buffer)[m_playbackPosition];
            data.sampleCount = std::min(static_cast<size_t>(CHUNK_SIZE), curr_mix_buffer->size() - m_playbackPosition);

            m_playbackPosition += data.sampleCount;
            m_totalFramesElapsed += data.sampleCount / m_channels;

            return true;
        } else {
            m_playbackPosition = 0;
            return false;
        }
    }

    virtual void onSeek(sf::Time timeOffset) override {
        // Seek to a specific position
        size_t targetFrame = static_cast<size_t>(timeOffset.asSeconds() * m_sampleRate);
        m_playbackPosition = targetFrame * m_channels;
    }

private:
    AudioStreamBuffer mix_buffer_A;
    LoopingStatistics looping_stats_A;

    AudioStreamBuffer mix_buffer_B;
    LoopingStatistics looping_stats_B;

    const AudioStreamBuffer* curr_mix_buffer; // Pointer to the active buffer (either A or B)
    const LoopingStatistics* curr_looping_stats;

    unsigned int m_sampleRate = 44100; // Audio sample rate
    unsigned int m_channels = 2;   // Number of channels

    size_t m_playbackPosition = 0; // Current position in the active buffer
    size_t m_totalFramesElapsed = 0;  // Total frames elapsed in the current interval since last switch

    std::mutex mtx;
    int curr_buffer_index = 0;
    bool dirty = false;

    LoopingStatistics looping_stats;
    size_t CHUNK_SIZE; // Number of samples per chunk

    void swapIntermediateIntoCurrentBuffer() {
        if (curr_buffer_index == 0) {
            curr_mix_buffer = &mix_buffer_B;
            curr_looping_stats = &looping_stats_B;
        } else {
            curr_mix_buffer = &mix_buffer_A;
            curr_looping_stats = &looping_stats_A;
        }

        curr_buffer_index = (curr_buffer_index + 1) % 2;
        dirty = false;
    }
};