#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "AudioProcessor.h"

#include <cmath>
#include <algorithm>
#include <numeric>
#include <cstdio>

std::optional<AudioBuffer> AudioProcessor::loadAudioFile(const std::filesystem::path& filePath) {
    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 2, 48000);
    ma_decoder decoder;

    if (ma_decoder_init_file(filePath.string().c_str(), &config, &decoder) != MA_SUCCESS) {
        return std::nullopt;
    }

    ma_uint64 totalFrames = 0;
    if (ma_decoder_get_length_in_pcm_frames(&decoder, &totalFrames) != MA_SUCCESS) {
        ma_decoder_uninit(&decoder);
        return std::nullopt;
    }

    AudioBuffer audio;
    audio.channels = 2;
    audio.sampleRate = 48000;
    audio.totalFrames = totalFrames;
    audio.samples.resize(totalFrames * audio.channels);

    ma_uint64 framesRead = 0;
    ma_decoder_read_pcm_frames(&decoder, audio.samples.data(), totalFrames, &framesRead);
    ma_decoder_uninit(&decoder);

    return audio;
}

bool AudioProcessor::saveWavFile(const std::filesystem::path& filePath, const AudioBuffer& audio) {
    ma_encoder_config config = ma_encoder_config_init(
        ma_encoding_format_wav,
        ma_format_f32,
        audio.channels,
        audio.sampleRate
    );
    ma_encoder encoder;

    if (ma_encoder_init_file(filePath.string().c_str(), &config, &encoder) != MA_SUCCESS) {
        return false;
    }

    ma_encoder_write_pcm_frames(&encoder, audio.samples.data(), audio.totalFrames, nullptr);
    ma_encoder_uninit(&encoder);
    return true;
}

bool AudioProcessor::saveAudioFile(const std::filesystem::path& filePath, const AudioBuffer& audio) {
    return saveWavFile(filePath, audio);
}

double AudioProcessor::calculateRMSdB(const AudioBuffer& audio) {
    if (audio.samples.empty()) return -100.0;

    double sumSquared = std::accumulate(audio.samples.begin(), audio.samples.end(), 0.0,
        [](double sum, float sample) {
            return sum + (static_cast<double>(sample) * sample);
        });

    double meanSquare = sumSquared / audio.samples.size();
    if (meanSquare <= 1e-12) return -100.0;

    return 20.0 * std::log10(std::sqrt(meanSquare));
}

ProcessingResult AudioProcessor::processAndMatch(
    const std::filesystem::path& targetPath,
    const std::filesystem::path& refPath,
    const std::filesystem::path& outputPath
) {
    auto targetAudio = loadAudioFile(targetPath);
    if (!targetAudio) {
        return {false, 0.0, 0.0, 0.0, "Failed to decode target audio file."};
    }

    auto refAudio = loadAudioFile(refPath);
    if (!refAudio) {
        return {false, 0.0, 0.0, 0.0, "Failed to decode reference audio file."};
    }

    double refDb = calculateRMSdB(*refAudio);
    double targetDb = calculateRMSdB(*targetAudio);
    double gainDb = refDb - targetDb;

    float scaleFactor = static_cast<float>(std::pow(10.0, gainDb / 20.0));

    for (float& sample : targetAudio->samples) {
        float scaled = sample * scaleFactor;
        if (std::abs(scaled) > 1.0f) {
            scaled = std::tanh(scaled);
        }
        sample = scaled;
    }

    if (!saveAudioFile(outputPath, *targetAudio)) {
        return {false, refDb, targetDb, gainDb, "Failed to write output audio file."};
    }

    return {true, refDb, targetDb, gainDb, "Matching completed successfully."};
}