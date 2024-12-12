#ifndef AUDIO_UTILS_H
#define AUDIO_UTILS_H

#include <SFML/Audio.hpp>
#include <iostream>
#include <vector>

void loadWavFile(const std::string& filename, std::vector<sf::Int16>& buffer, int& sampleRate, int& channels) {
    sf::SoundBuffer soundBuffer;
    if (!soundBuffer.loadFromFile(filename)) {
        std::cerr << "Failed to load " << filename << "\n";
        return;
    }

    // Get audio properties and samples
    sampleRate = soundBuffer.getSampleRate();
    channels = soundBuffer.getChannelCount();
    const sf::Int16* samples = soundBuffer.getSamples();
    buffer.assign(samples, samples + soundBuffer.getSampleCount());

    std::cout << "Loaded " << filename << " with " << soundBuffer.getSampleCount() << " samples, "
              << sampleRate << " Hz, " << channels << " channels.\n";
}

void mixSample(std::vector<sf::Int16>& mix, const std::vector<sf::Int16>& sample, int start, float volume = 0.75f) {
    for (size_t i = 0; i < sample.size(); i += 2) {
        int mixIndexL = 2 * (start + i / 2);
        int mixIndexR = 2 * (start + i / 2) + 1;

        if (mixIndexR < mix.size()) {
            mix[mixIndexL] += static_cast<sf::Int16>(sample[i] * volume);
            mix[mixIndexR] += static_cast<sf::Int16>(sample[i + 1] * volume);

            // Prevent clipping
            mix[mixIndexL] = std::max(static_cast<sf::Int16>(-32768), std::min(static_cast<sf::Int16>(32767), mix[mixIndexL]));
            mix[mixIndexR] = std::max(static_cast<sf::Int16>(-32768), std::min(static_cast<sf::Int16>(32767), mix[mixIndexR]));
        } else break;
    }
}

void unmixSample(std::vector<sf::Int16>& mix, const std::vector<sf::Int16>& sample, int start, float volume = 0.75f) {
    for (size_t i = 0; i < sample.size(); i += 2) {
        int mixIndexL = 2 * (start + i / 2);
        int mixIndexR = 2 * (start + i / 2) + 1;

        if (mixIndexR < mix.size()) {
            mix[mixIndexL] -= static_cast<sf::Int16>(sample[i] * volume);
            mix[mixIndexR] -= static_cast<sf::Int16>(sample[i + 1] * volume);

            // Prevent clipping
            mix[mixIndexL] = std::max(static_cast<sf::Int16>(-32768), std::min(static_cast<sf::Int16>(32767), mix[mixIndexL]));
            mix[mixIndexR] = std::max(static_cast<sf::Int16>(-32768), std::min(static_cast<sf::Int16>(32767), mix[mixIndexR]));
        } else break;
    }
}

#endif