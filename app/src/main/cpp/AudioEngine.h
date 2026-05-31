#ifndef VOICECHANGER_AUDIOENGINE_H
#define VOICECHANGER_AUDIOENGINE_H

#include <aaudio/AAudio.h>
#include <vector>
#include <atomic>
#include <string>
#include <fstream>
#include <mutex>

// Struktura za pamćenje prethodnih semplova Biquad filtera
struct BiquadState {
    float x1 = 0.0f, x2 = 0.0f;
    float y1 = 0.0f, y2 = 0.0f;
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f;
};

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    bool start();
    void stop();

    // Snimanje
    void startRecording(const char* filePath);
    void stopRecording();

    // PRO 5-Band EQ (Gain u dB, Freq u Hz, Q faktor)
    void setEqBand(int bandIndex, float gainDb, float freqHz, float q);
    void setEqEnabled(bool enabled);

    // Ostali Efekti
    void setMorphParams(bool enabled, float pitchShift);
    void setDelayParams(bool enabled, float timeL, float timeR, float feedback, float mix);
    void setReverbParams(bool enabled, float size, float damping, float mix);

    // UI Feedback
    float getCurrentAmplitude();

    aaudio_data_callback_result_t onAudioReady(AAudioStream *stream, void *audioData, int32_t numFrames);

private:
    bool openRecordingStream();
    bool openPlaybackStream();
    void processAudio(float* inputBuffer, float* outputBuffer, int32_t numFrames);

    // Matematika za EQ
    void calculateBiquadCoeffs(int bandIndex, float freq, float gainDb, float q, int type);
    float processBiquad(float input, BiquadState& state);

    AAudioStream* recordingStream_ = nullptr;
    AAudioStream* playbackStream_ = nullptr;
    int32_t sampleRate_ = 48000;

    // Parametri efekata
    std::atomic<bool> eqEnabled_{false};
    BiquadState eqBands_[5]; // 0:LowShelf, 1:LowMid, 2:Mid, 3:HighMid, 4:HighShelf

    std::atomic<bool> morphEnabled_{false};
    std::atomic<float> morphShift_{1.0f};

    std::atomic<bool> delayEnabled_{false};
    std::atomic<float> delayTimeL_{0.3f};
    std::atomic<float> delayTimeR_{0.4f};
    std::atomic<float> delayFeedback_{0.5f};
    std::atomic<float> delayMix_{0.5f};

    std::atomic<bool> reverbEnabled_{false};
    std::atomic<float> reverbSize_{0.8f};
    std::atomic<float> reverbDamping_{0.5f};
    std::atomic<float> reverbMix_{0.3f};

    std::atomic<bool> isRecording_{false};
    std::ofstream wavFile_;
    std::string currentWavPath_;
    uint32_t totalFramesWritten_ = 0;
    std::atomic<float> currentAmplitude_{0.0f};

    // Baferi za Delay i Reverb
    std::vector<float> delayBufferL_;
    std::vector<float> delayBufferR_;
    int delayWriteIndex_ = 0;

    // Reverb Comb Filter Baferi
    std::vector<float> comb1_, comb2_, comb3_, comb4_;
    int cIdx1_ = 0, cIdx2_ = 0, cIdx3_ = 0, cIdx4_ = 0;

    // Reverb Allpass Filter Baferi
    std::vector<float> ap1_, ap2_;
    int aIdx1_ = 0, aIdx2_ = 0;

    void writeWavHeader();
    void updateWavHeader();
    void initReverbBuffers();
};

#endif //VOICECHANGER_AUDIOENGINE_H