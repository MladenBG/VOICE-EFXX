#include "AudioEngine.h"
#include <android/log.h>
#include <cmath>
#include <algorithm>

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "AudioEngine", __VA_ARGS__)

// WAV Header structure (Stereo)
#pragma pack(push, 1)
struct WavHeader {
    char riff[4] = {'R','I','F','F'};
    uint32_t chunkSize = 0;
    char wave[4] = {'W','A','V','E'};
    char fmt[4] = {'f','m','t',' '};
    uint32_t fmtSize = 16;
    uint16_t audioFormat = 1;
    uint16_t numChannels = 2;
    uint32_t sampleRate = 48000;
    uint32_t byteRate = 48000 * 2 * sizeof(int16_t);
    uint16_t blockAlign = 2 * sizeof(int16_t);
    uint16_t bitsPerSample = 16;
    char data[4] = {'d','a','t','a'};
    uint32_t dataSize = 0;
};
#pragma pack(pop)

AudioEngine::AudioEngine() {
    // Initialize STK Global Sample Rate
    stk::Stk::setSampleRate(48000.0);

    // Initialize SoundTouch
    soundTouch_.setSampleRate(48000);
    soundTouch_.setChannels(1); // Pitch shift in Mono
    soundTouch_.setTempoChange(0.0f);
    soundTouch_.setPitchSemiTones(0.0f);
    soundTouch_.setSetting(SETTING_USE_QUICKSEEK, 0);
    soundTouch_.setSetting(SETTING_USE_AA_FILTER, 1);

    // Initialize STK effects
    stkDelay_.setMaximumDelay(48000); // 1 second max
    stkDelay_.setDelay(14400); // 300ms default (48000 * 0.3)
}

AudioEngine::~AudioEngine() {
    stop();
    stopRecording();
}

void AudioEngine::setPitchParams(bool enabled, float pitchSemitones) {
    std::lock_guard<std::mutex> lock(dspMutex_);
    pitchEnabled_ = enabled;
    currentPitch_ = pitchSemitones;
    soundTouch_.setPitchSemiTones(currentPitch_.load());
}

void AudioEngine::setChorusParams(bool enabled, float modDepth, float modFrequency) {
    std::lock_guard<std::mutex> lock(dspMutex_);
    chorusEnabled_ = enabled;
    chorusDepth_ = modDepth;
    chorusFreq_ = modFrequency;
    stkChorus_.setModDepth(chorusDepth_.load());
    stkChorus_.setModFrequency(chorusFreq_.load());
}

void AudioEngine::setDelayParams(bool enabled, float delayMs, float feedback) {
    std::lock_guard<std::mutex> lock(dspMutex_);
    delayEnabled_ = enabled;
    delayTime_ = delayMs;
    delayFeedback_ = feedback;

    // Convert milliseconds to samples
    float delaySamples = (delayTime_.load() / 1000.0f) * 48000.0f;
    stkDelay_.setDelay(delaySamples);
}

float AudioEngine::getCurrentAmplitude() {
    return currentAmplitude_.load();
}

void AudioEngine::startRecording(const char* filePath) {
    if (isRecording_) stopRecording();
    currentWavPath_ = filePath;
    wavFile_.open(currentWavPath_, std::ios::binary);

    if (wavFile_.is_open()) {
        totalFramesWritten_ = 0;
        writeWavHeader();
        isRecording_ = true;
    }
}

void AudioEngine::stopRecording() {
    if (isRecording_ && wavFile_.is_open()) {
        isRecording_ = false;
        updateWavHeader();
        wavFile_.close();
    }
}

void AudioEngine::writeWavHeader() {
    WavHeader header;
    wavFile_.seekp(0, std::ios::beg);
    wavFile_.write(reinterpret_cast<const char*>(&header), sizeof(WavHeader));
}

void AudioEngine::updateWavHeader() {
    WavHeader header;
    header.dataSize = totalFramesWritten_ * 2 * sizeof(int16_t);
    header.chunkSize = 36 + header.dataSize;
    wavFile_.seekp(0, std::ios::beg);
    wavFile_.write(reinterpret_cast<const char*>(&header), sizeof(WavHeader));
}

bool AudioEngine::start() {
    oboe::AudioStreamBuilder builder;

    // Input Stream (Mic)
    builder.setDirection(oboe::Direction::Input);
    builder.setPerformanceMode(oboe::PerformanceMode::LowLatency);
    builder.setFormat(oboe::AudioFormat::Float);
    builder.setChannelCount(1); // Mono input

    oboe::Result result = builder.openStream(recordingStream_);
    if (result != oboe::Result::OK) {
        LOGE("Failed to open recording stream.");
        return false;
    }

    sampleRate_ = recordingStream_->getSampleRate();
    stk::Stk::setSampleRate(sampleRate_);
    soundTouch_.setSampleRate(sampleRate_);

    // Output Stream (Speaker)
    builder.setDirection(oboe::Direction::Output);
    builder.setDataCallback(this); // DODATO 'Data'
    builder.setFormat(oboe::AudioFormat::Float);
    builder.setChannelCount(2); // Stereo output

    result = builder.openStream(playbackStream_);
    if (result != oboe::Result::OK) {
        LOGE("Failed to open playback stream.");
        return false;
    }

    playbackStream_->requestStart();
    recordingStream_->requestStart();

    return true;
}

void AudioEngine::stop() {
    if (recordingStream_) {
        recordingStream_->requestStop();
        recordingStream_->close();
        recordingStream_.reset();
    }
    if (playbackStream_) {
        playbackStream_->requestStop();
        playbackStream_->close();
        playbackStream_.reset();
    }
}

oboe::DataCallbackResult AudioEngine::onAudioReady(oboe::AudioStream *oboeStream, void *audioData, int32_t numFrames) {
    auto *outputBuffer = static_cast<float *>(audioData);
    float inputBuffer[numFrames];

    auto result = recordingStream_->read(inputBuffer, numFrames, 0);

    if (!result || result.value() == 0) {
        for(int i = 0; i < numFrames * 2; i++) outputBuffer[i] = 0.0f;
        return oboe::DataCallbackResult::Continue;
    }

    processAudio(inputBuffer, outputBuffer, result.value());
    return oboe::DataCallbackResult::Continue;
}

void AudioEngine::processAudio(float* inputBuffer, float* outputBuffer, int32_t numFrames) {
    std::lock_guard<std::mutex> lock(dspMutex_);
    float peakAmp = 0.0f;
    std::vector<int16_t> wavData;

    if (isRecording_) {
        wavData.reserve(numFrames * 2);
    }

    // Processing buffers
    std::vector<float> processBuffer(inputBuffer, inputBuffer + numFrames);

    // 1. SOUNDTOUCH PITCH SHIFT
    if (pitchEnabled_.load()) {
        soundTouch_.putSamples(processBuffer.data(), numFrames);
        int samplesReady = soundTouch_.numSamples();
        if (samplesReady >= numFrames) {
            soundTouch_.receiveSamples(processBuffer.data(), numFrames);
        } else {
            // If SoundTouch changes tempo, pad with zeros (basic fallback)
            soundTouch_.receiveSamples(processBuffer.data(), samplesReady);
            for(int i = samplesReady; i < numFrames; i++) processBuffer[i] = 0.0f;
        }
    } else {
        soundTouch_.clear(); // Clear internal buffer if disabled
    }

    // Process through STK and prepare for Stereo output
    for (int i = 0; i < numFrames; i++) {
        float sample = processBuffer[i];

        if (std::abs(sample) > peakAmp) peakAmp = std::abs(sample);

        // 2. STK CHORUS
        if (chorusEnabled_.load()) {
            sample = stkChorus_.tick(sample);
        }

        // 3. STK ECHO (Delay)
        if (delayEnabled_.load()) {
            // STK Echo mixes dry and wet signal. Handle feedback manually.
            float delayed = stkDelay_.lastOut();
            stkDelay_.tick(sample + (delayed * delayFeedback_.load()));
            sample = (sample * 0.5f) + (delayed * 0.5f);
        }

        // Hard clipping prevention (Limiter)
        sample = std::clamp(sample, -1.0f, 1.0f);

        // Stereo Output copying
        outputBuffer[i * 2] = sample;       // L
        outputBuffer[i * 2 + 1] = sample;   // R

        if (isRecording_) {
            wavData.push_back(static_cast<int16_t>(sample * 32767.0f)); // L
            wavData.push_back(static_cast<int16_t>(sample * 32767.0f)); // R
        }
    }

    currentAmplitude_.store(peakAmp);

    if (isRecording_ && wavFile_.is_open()) {
        wavFile_.write(reinterpret_cast<const char*>(wavData.data()), wavData.size() * sizeof(int16_t));
        totalFramesWritten_ += (wavData.size() / 2);
    }
}