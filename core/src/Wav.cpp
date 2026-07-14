#include "bpmcore/Wav.hpp"

#include <cstring>
#include <fstream>
#include <stdexcept>

namespace bpmcore {

namespace {

uint32_t readU32LE(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

uint16_t readU16LE(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

void writeU32LE(std::ofstream& out, uint32_t v) {
    uint8_t b[4] = {static_cast<uint8_t>(v & 0xFF), static_cast<uint8_t>((v >> 8) & 0xFF),
                    static_cast<uint8_t>((v >> 16) & 0xFF), static_cast<uint8_t>((v >> 24) & 0xFF)};
    out.write(reinterpret_cast<const char*>(b), 4);
}

void writeU16LE(std::ofstream& out, uint16_t v) {
    uint8_t b[2] = {static_cast<uint8_t>(v & 0xFF), static_cast<uint8_t>((v >> 8) & 0xFF)};
    out.write(reinterpret_cast<const char*>(b), 2);
}

} // namespace

WavData readWavMono(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("bpmcore: cannot open WAV file: " + path);
    }

    std::vector<uint8_t> buf((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (buf.size() < 12 || std::memcmp(buf.data(), "RIFF", 4) != 0 || std::memcmp(buf.data() + 8, "WAVE", 4) != 0) {
        throw std::runtime_error("bpmcore: not a RIFF/WAVE file: " + path);
    }

    uint16_t audioFormat = 0;
    uint16_t numChannels = 0;
    uint32_t sampleRate = 0;
    uint16_t bitsPerSample = 0;
    const uint8_t* dataPtr = nullptr;
    uint32_t dataSize = 0;

    size_t pos = 12;
    bool haveFmt = false;
    while (pos + 8 <= buf.size()) {
        char chunkId[5] = {0};
        std::memcpy(chunkId, buf.data() + pos, 4);
        uint32_t chunkSize = readU32LE(buf.data() + pos + 4);
        size_t chunkDataStart = pos + 8;
        if (chunkDataStart + chunkSize > buf.size()) {
            break;
        }
        if (std::memcmp(chunkId, "fmt ", 4) == 0) {
            const uint8_t* p = buf.data() + chunkDataStart;
            audioFormat = readU16LE(p + 0);
            numChannels = readU16LE(p + 2);
            sampleRate = readU32LE(p + 4);
            bitsPerSample = readU16LE(p + 14);
            haveFmt = true;
        } else if (std::memcmp(chunkId, "data", 4) == 0) {
            dataPtr = buf.data() + chunkDataStart;
            dataSize = chunkSize;
        }
        pos = chunkDataStart + chunkSize + (chunkSize & 1); // chunks are word-aligned
    }

    if (!haveFmt || dataPtr == nullptr) {
        throw std::runtime_error("bpmcore: missing fmt/data chunk in WAV file: " + path);
    }
    if (numChannels == 0) {
        throw std::runtime_error("bpmcore: invalid channel count in WAV file: " + path);
    }
    // audioFormat: 1 = PCM integer, 3 = IEEE float
    if (audioFormat != 1 && audioFormat != 3) {
        throw std::runtime_error("bpmcore: unsupported WAV audio format (only PCM/float supported): " + path);
    }

    const size_t bytesPerSample = bitsPerSample / 8;
    if (bytesPerSample == 0) {
        throw std::runtime_error("bpmcore: invalid bits-per-sample in WAV file: " + path);
    }
    const size_t frameBytes = bytesPerSample * numChannels;
    const size_t numFrames = frameBytes > 0 ? dataSize / frameBytes : 0;

    WavData out;
    out.sampleRate = static_cast<double>(sampleRate);
    out.samples.resize(numFrames);

    for (size_t frame = 0; frame < numFrames; ++frame) {
        float mixed = 0.0f;
        const uint8_t* base = dataPtr + frame * frameBytes;
        for (uint16_t ch = 0; ch < numChannels; ++ch) {
            const uint8_t* s = base + ch * bytesPerSample;
            float v = 0.0f;
            if (audioFormat == 3 && bitsPerSample == 32) {
                float f;
                std::memcpy(&f, s, 4);
                v = f;
            } else if (audioFormat == 1 && bitsPerSample == 16) {
                int16_t i;
                std::memcpy(&i, s, 2);
                v = static_cast<float>(i) / 32768.0f;
            } else if (audioFormat == 1 && bitsPerSample == 8) {
                // 8-bit PCM is unsigned in WAV
                v = (static_cast<float>(s[0]) - 128.0f) / 128.0f;
            } else if (audioFormat == 1 && bitsPerSample == 32) {
                int32_t i;
                std::memcpy(&i, s, 4);
                v = static_cast<float>(i) / 2147483648.0f;
            } else if (audioFormat == 1 && bitsPerSample == 24) {
                int32_t i = (static_cast<int32_t>(s[0]) << 8) | (static_cast<int32_t>(s[1]) << 16) |
                            (static_cast<int32_t>(s[2]) << 24);
                v = static_cast<float>(i >> 8) / 8388608.0f;
            } else {
                throw std::runtime_error("bpmcore: unsupported sample format in WAV file: " + path);
            }
            mixed += v;
        }
        out.samples[frame] = mixed / static_cast<float>(numChannels);
    }

    return out;
}

void writeWavMonoPCM16(const std::string& path, double sampleRate, const std::vector<float>& samples) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("bpmcore: cannot open WAV file for writing: " + path);
    }

    const uint16_t numChannels = 1;
    const uint16_t bitsPerSample = 16;
    const uint32_t sr = static_cast<uint32_t>(sampleRate);
    const uint32_t byteRate = sr * numChannels * bitsPerSample / 8;
    const uint16_t blockAlign = numChannels * bitsPerSample / 8;
    const uint32_t dataSize = static_cast<uint32_t>(samples.size() * sizeof(int16_t));
    const uint32_t riffSize = 36 + dataSize;

    out.write("RIFF", 4);
    writeU32LE(out, riffSize);
    out.write("WAVE", 4);

    out.write("fmt ", 4);
    writeU32LE(out, 16);
    writeU16LE(out, 1); // PCM
    writeU16LE(out, numChannels);
    writeU32LE(out, sr);
    writeU32LE(out, byteRate);
    writeU16LE(out, blockAlign);
    writeU16LE(out, bitsPerSample);

    out.write("data", 4);
    writeU32LE(out, dataSize);

    for (float s : samples) {
        float clamped = s < -1.0f ? -1.0f : (s > 1.0f ? 1.0f : s);
        int16_t i = static_cast<int16_t>(clamped * 32767.0f);
        out.write(reinterpret_cast<const char*>(&i), sizeof(int16_t));
    }
}

} // namespace bpmcore
