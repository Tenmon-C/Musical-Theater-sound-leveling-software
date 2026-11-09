#pragma once

#include <filesystem>
#include <vector>
#include <cstdint>
#include <optional>
#include <string>

struct AudioBuffer {
    std::vector<float> samples;
    uint32_t channels{2};
    uint32_t sampleRate{48000};
    uint64_t totalFrames{0};
};

struct ProcessingResult {
    bool success{false};
    double refDb{0.0};
    double targetDb{0.0};
    double gainDb{0.0};
    std::string errorMessage;
};

class AudioProcessor {
public:
    std::optional<AudioBuffer> loadAudioFile(const std::filesystem::path& filePath);
    bool saveWavFile(const std::filesystem::path& filePath, const AudioBuffer& audio);
    bool saveAudioFile(const std::filesystem::path& filePath, const AudioBuffer& audio);
    double calculateRMSdB(const AudioBuffer& audio);

    ProcessingResult processAndMatch(const std::filesystem::path& targetPath,
                                     const std::filesystem::path& refPath,
                                     const std::filesystem::path& outputPath);
};