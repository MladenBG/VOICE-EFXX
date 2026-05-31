#include "AudioEngine.h"
#include <android/log.h>
#include <cmath>
#include <algorithm>

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "AudioEngine", __VA_ARGS__)

const float PI = 3.14159265358979323846f;

aaudio_data_callback_result_t dataCallbackWrapper(AAudioStream *stream, void *userData, void *audioData, int32_t numFrames) {
    auto *engine = static_cast<AudioEngine *>(userData);
    return engine->onAudioReady(stream, audioData, numFrames);
}

#pragma pack(push, 1)
struct WavHeader {
    char riff[4] = {'R','I','F','F'};
    uint32_t chunkSize = 0;
    char wave[4] = {'W','A','V','E'};
    char fmt[4] = {'f','m','t',' '};
    uint32_t fmtSize = 16;
    uint16_t audioFormat = 1;
    uint16_t numChannels = 2; // Stereo
    uint32_t sampleRate = 48000;
    uint32_t byteRate = 48000 * 2 * sizeof(int16_t);
    uint16_t blockAlign = 2 * sizeof(int16_t);
    uint16_t bitsPerSample = 16;
    char data[4] = {'d','a','t','a'};
    uint32_t dataSize = 0;
};
#pragma pack(pop)

AudioEngine::AudioEngine() {
    delayBufferL_.resize(48000 * 2, 0.0f);
    delayBufferR_.resize(48000 * 2, 0.0f);
    initReverbBuffers();

    // Default EQ Postavke (Flat)
    // 0: LowShelf, 1: Peaking, 2: Peaking, 3: Peaking, 4: HighShelf
    setEqBand(0, 0.0f, 100.0f, 0.707f);
    setEqBand(1, 0.0f, 400.0f, 1.0f);
    setEqBand(2, 0.0f, 1000.0f, 1.0f);
    setEqBand(3, 0.0f, 4000.0f, 1.0f);
    setEqBand(4, 0.0f, 10000.0f, 0.707f);
}

AudioEngine::~AudioEngine() {
    stop();
    stopRecording();
}

void AudioEngine::initReverbBuffers() {
    // Schroeder Reverb vremena u semplovima (za 48kHz)
    comb1_.resize(1424, 0.0f); // 29.7ms
    comb2_.resize(1781, 0.0f); // 37.1ms
    comb3_.resize(1973, 0.0f); // 41.1ms
    comb4_.resize(2076, 0.0f); // 43.2ms
    ap1_.resize(240, 0.0f);    // 5.0ms
    ap2_.resize(82, 0.0f);     // 1.7ms
}

// --- BIQUAD MATH (Studijski EQ Algoritam) ---
// type: 0 = LowShelf, 1 = Peaking, 2 = HighShelf
void AudioEngine::calculateBiquadCoeffs(int bandIndex, float freq, float gainDb, float q, int type) {
    if (bandIndex < 0 || bandIndex > 4) return;

    float A = powf(10.0f, gainDb / 40.0f);
    float w0 = 2.0f * PI * freq / sampleRate_;
    float alpha = sinf(w0) / (2.0f * q);

    float b0, b1, b2, a0, a1, a2;

    switch (type) {
        case 0: // Low Shelf
            b0 = A * ((A + 1.0f) - (A - 1.0f) * cosf(w0) + 2.0f * sqrtf(A) * alpha);
            b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosf(w0));
            b2 = A * ((A + 1.0f) - (A - 1.0f) * cosf(w0) - 2.0f * sqrtf(A) * alpha);
            a0 = (A + 1.0f) + (A - 1.0f) * cosf(w0) + 2.0f * sqrtf(A) * alpha;
            a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosf(w0));
            a2 = (A + 1.0f) + (A - 1.0f) * cosf(w0) - 2.0f * sqrtf(A) * alpha;
            break;
        case 1: // Peaking (Bell)
            b0 = 1.0f + alpha * A;
            b1 = -2.0f * cosf(w0);
            b2 = 1.0f - alpha * A;
            a0 = 1.0f + alpha / A;
            a1 = -2.0f * cosf(w0);
            a2 = 1.0f - alpha / A;
            break;
        case 2: // High Shelf
            b0 = A * ((A + 1.0f) + (A - 1.0f) * cosf(w0) + 2.0f * sqrtf(A) * alpha);
            b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosf(w0));
            b2 = A * ((A + 1.0f) + (A - 1.0f) * cosf(w0) - 2.0f * sqrtf(A) * alpha);
            a0 = (A + 1.0f) - (A - 1.0f) * cosf(w0) + 2.0f * sqrtf(A) * alpha;
            a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cosf(w0));
            a2 = (A + 1.0f) - (A - 1.0f) * cosf(w0) - 2.0f * sqrtf(A) * alpha;
            break;
    }

    // Normalizacija koeficijenata
    eqBands_[bandIndex].b0 = b0 / a0;
    eqBands_[bandIndex].b1 = b1 / a0;
    eqBands_[bandIndex].b2 = b2 / a0;
    eqBands_[bandIndex].a1 = a1 / a0;
    eqBands_[bandIndex].a2 = a2 / a0;
}

float AudioEngine::processBiquad(float input, BiquadState& state) {
    float output = state.b0 * input + state.b1 * state.x1 + state.b2 * state.x2 - state.a1 * state.y1 - state.a2 * state.y2;
    // Pomeranje memorije
    state.x2 = state.x1;
    state.x1 = input;
    state.y2 = state.y1;
    state.y1 = output;
    return output;
}

void AudioEngine::setEqBand(int bandIndex, float gainDb, float freqHz, float q) {
    int type = 1; // Peaking by default
    if (bandIndex == 0) type = 0; // LowShelf za prvi
    if (bandIndex == 4) type = 2; // HighShelf za poslednji
    calculateBiquadCoeffs(bandIndex, freqHz, gainDb, q, type);
}

void AudioEngine::setEqEnabled(bool enabled) { eqEnabled_ = enabled; }
void AudioEngine::setMorphParams(bool enabled, float pitchShift) { morphEnabled_ = enabled; morphShift_ = pitchShift; }
void AudioEngine::setDelayParams(bool enabled, float timeL, float timeR, float feedback, float mix) { delayEnabled_ = enabled; delayTimeL_ = timeL; delayTimeR_ = timeR; delayFeedback_ = feedback; delayMix_ = mix; }
void AudioEngine::setReverbParams(bool enabled, float size, float damping, float mix) { reverbEnabled_ = enabled; reverbSize_ = size; reverbDamping_ = damping; reverbMix_ = mix; }
float AudioEngine::getCurrentAmplitude() { return currentAmplitude_.load(); }

// ... [OSTATAK RECORDING LOGIKE OSTAJE ISTI KAO U PRETHODNOM KODU: startRecording, writeWavHeader, start, stop, open streams] ...
// (Da ne dupliramo nepromenjene delove, ovde idu start/stop metode za streams i wav headers koje smo definisali gore)

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

void AudioEngine::writeWavHeader() { WavHeader header; wavFile_.seekp(0, std::ios::beg); wavFile_.write(reinterpret_cast<const char*>(&header), sizeof(WavHeader)); }
void AudioEngine::updateWavHeader() { WavHeader header; header.dataSize = totalFramesWritten_ * 2 * sizeof(int16_t); header.chunkSize = 36 + header.dataSize; wavFile_.seekp(0, std::ios::beg); wavFile_.write(reinterpret_cast<const char*>(&header), sizeof(WavHeader)); }

bool AudioEngine::start() { if (!openRecordingStream() || !openPlaybackStream()) return false; AAudioStream_requestStart(playbackStream_); AAudioStream_requestStart(recordingStream_); return true; }
void AudioEngine::stop() { if (recordingStream_) { AAudioStream_requestStop(recordingStream_); AAudioStream_close(recordingStream_); recordingStream_ = nullptr; } if (playbackStream_) { AAudioStream_requestStop(playbackStream_); AAudioStream_close(playbackStream_); playbackStream_ = nullptr; } }

bool AudioEngine::openRecordingStream() { AAudioStreamBuilder *b; AAudio_createStreamBuilder(&b); AAudioStreamBuilder_setDirection(b, AAUDIO_DIRECTION_INPUT); AAudioStreamBuilder_setPerformanceMode(b, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY); AAudioStreamBuilder_setFormat(b, AAUDIO_FORMAT_PCM_FLOAT); AAudioStreamBuilder_setChannelCount(b, 1); aaudio_result_t r = AAudioStreamBuilder_openStream(b, &recordingStream_); AAudioStreamBuilder_delete(b); if (r == AAUDIO_OK) sampleRate_ = AAudioStream_getSampleRate(recordingStream_); return r == AAUDIO_OK; }
bool AudioEngine::openPlaybackStream() { AAudioStreamBuilder *b; AAudio_createStreamBuilder(&b); AAudioStreamBuilder_setDirection(b, AAUDIO_DIRECTION_OUTPUT); AAudioStreamBuilder_setPerformanceMode(b, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY); AAudioStreamBuilder_setFormat(b, AAUDIO_FORMAT_PCM_FLOAT); AAudioStreamBuilder_setChannelCount(b, 2); AAudioStreamBuilder_setDataCallback(b, dataCallbackWrapper, this); aaudio_result_t r = AAudioStreamBuilder_openStream(b, &playbackStream_); AAudioStreamBuilder_delete(b); return r == AAUDIO_OK; }

aaudio_data_callback_result_t AudioEngine::onAudioReady(AAudioStream *stream, void *audioData, int32_t numFrames) {
    auto *outputBuffer = static_cast<float *>(audioData);
    float inputBuffer[numFrames];
    aaudio_result_t framesRead = AAudioStream_read(recordingStream_, inputBuffer, numFrames, 0);
    if (framesRead < 0) { for (int i = 0; i < numFrames * 2; i++) outputBuffer[i] = 0.0f; return AAUDIO_CALLBACK_RESULT_CONTINUE; }
    processAudio(inputBuffer, outputBuffer, framesRead);
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

// --- GLAVNI DSP LOOP ---
void AudioEngine::processAudio(float* inputBuffer, float* outputBuffer, int32_t numFrames) {
    float peakAmp = 0.0f;
    std::vector<int16_t> wavData;
    if (isRecording_) wavData.reserve(numFrames * 2);

    for (int i = 0; i < numFrames; i++) {
        float sample = inputBuffer[i];
        if (std::abs(sample) > peakAmp) peakAmp = std::abs(sample);

        // 1. PRO 5-BAND EQ
        if (eqEnabled_.load()) {
            sample = processBiquad(sample, eqBands_[0]);
            sample = processBiquad(sample, eqBands_[1]);
            sample = processBiquad(sample, eqBands_[2]);
            sample = processBiquad(sample, eqBands_[3]);
            sample = processBiquad(sample, eqBands_[4]);
        }

        // 2. MORPH
        if (morphEnabled_.load()) {
            int shift = (int)(morphShift_.load() * 1000.0f);
            int dS = (delayWriteIndex_ * 2) % std::max(10, shift);
            int rI = delayWriteIndex_ - dS;
            if (rI < 0) rI += delayBufferL_.size();
            sample = (sample + delayBufferL_[rI]) * 0.5f;
        }

        float outL = sample; float outR = sample;

        // 3. PRO STEREO DELAY
        if (delayEnabled_.load()) {
            int dSamplesL = (int)(sampleRate_ * delayTimeL_.load());
            int dSamplesR = (int)(sampleRate_ * delayTimeR_.load());

            int rIndexL = delayWriteIndex_ - dSamplesL; if (rIndexL < 0) rIndexL += delayBufferL_.size();
            int rIndexR = delayWriteIndex_ - dSamplesR; if (rIndexR < 0) rIndexR += delayBufferR_.size();

            float delayedL = delayBufferL_[rIndexL]; float delayedR = delayBufferR_[rIndexR];

            outL += delayedL * delayMix_.load(); outR += delayedR * delayMix_.load();
            delayBufferL_[delayWriteIndex_] = sample + (delayedR * delayFeedback_.load());
            delayBufferR_[delayWriteIndex_] = sample + (delayedL * delayFeedback_.load());
        } else {
            delayBufferL_[delayWriteIndex_] = sample; delayBufferR_[delayWriteIndex_] = sample;
        }

        // 4. PRO REVERB (Schroeder)
        if (reverbEnabled_.load()) {
            float revIn = (outL + outR) * 0.5f; // Mono in for reverb processing
            float f = reverbSize_.load(); // Feedback
            float d = reverbDamping_.load();

            // 4 paralelna comb filtera
            float outC1 = comb1_[cIdx1_]; comb1_[cIdx1_] = revIn + (outC1 * f * (1.0f - d)); cIdx1_ = (cIdx1_ + 1) % comb1_.size();
            float outC2 = comb2_[cIdx2_]; comb2_[cIdx2_] = revIn + (outC2 * f * (1.0f - d)); cIdx2_ = (cIdx2_ + 1) % comb2_.size();
            float outC3 = comb3_[cIdx3_]; comb3_[cIdx3_] = revIn + (outC3 * f * (1.0f - d)); cIdx3_ = (cIdx3_ + 1) % comb3_.size();
            float outC4 = comb4_[cIdx4_]; comb4_[cIdx4_] = revIn + (outC4 * f * (1.0f - d)); cIdx4_ = (cIdx4_ + 1) % comb4_.size();

            float combSum = (outC1 + outC2 + outC3 + outC4) * 0.25f;

            // 2 serijska Allpass filtera za difuziju
            float apG = 0.7f;
            float apOut1 = -apG * combSum + ap1_[aIdx1_]; ap1_[aIdx1_] = combSum + apG * ap1_[aIdx1_]; aIdx1_ = (aIdx1_ + 1) % ap1_.size();
            float apOut2 = -apG * apOut1 + ap2_[aIdx2_]; ap2_[aIdx2_] = apOut1 + apG * ap2_[aIdx2_]; aIdx2_ = (aIdx2_ + 1) % ap2_.size();

            // Dodaj reverb na originalni signal
            outL += apOut2 * reverbMix_.load();
            outR += apOut2 * reverbMix_.load();
        }

        // Soft Clipping zaštita od preglasnog distorziranja (umesto Hard Clippinga)
        outL = std::tanh(outL);
        outR = std::tanh(outR);

        outputBuffer[i * 2] = outL;
        outputBuffer[i * 2 + 1] = outR;

        if (isRecording_) {
            wavData.push_back(static_cast<int16_t>(outL * 32767.0f));
            wavData.push_back(static_cast<int16_t>(outR * 32767.0f));
        }

        delayWriteIndex_ = (delayWriteIndex_ + 1) % delayBufferL_.size();
    }

    currentAmplitude_.store(peakAmp);

    if (isRecording_ && wavFile_.is_open()) {
        wavFile_.write(reinterpret_cast<const char*>(wavData.data()), wavData.size() * sizeof(int16_t));
        totalFramesWritten_ += (wavData.size() / 2);
    }
}