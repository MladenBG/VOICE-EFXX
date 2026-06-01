#ifndef VOICECHANGER_AUDIOENGINE_H
#define VOICECHANGER_AUDIOENGINE_H

#include <oboe/Oboe.h>
#include <atomic>
#include <mutex>
#include <vector>
#include <string>
#include <fstream>
#include "SoundTouch.h"

// Structure for Biquad Filter Mathematics
struct BiquadState { float x1=0, x2=0, y1=0, y2=0, b0=1, b1=0, b2=0, a1=0, a2=0; };

class AudioEngine : public oboe::AudioStreamDataCallback {
public:
    AudioEngine();
    ~AudioEngine();

    bool start();
    void stop();

    void startRecording(const char* filePath);
    void stopRecording();

    // Time FX Parameters
    void setDelayParams(bool enabled, float timeL, float timeR, float feedback, float mix);
    void setReverbParams(bool enabled, float size, float damping, float mix);

    // Modulation FX Parameters
    void setChorusParams(bool enabled, float delayMs, float depth, float rate, float mix);
    void setFlangerParams(bool enabled, float delayMs, float depth, float rate, float feedback, float mix);
    void setPhaserParams(bool enabled, float rate, float depth, float feedback);
    void setAutoFilterParams(bool enabled, float cutoff, float res, float lfoRate, float lfoDepth);

    // Tone FX Parameters
    void setCompParams(bool enabled, float thresh, float ratio, float attack, float release);
    void setAmpParams(bool enabled, float drive, float tone, float output);
    void setEqBand(int bandIndex, float gainDb, float freqHz, float q);
    void setEqEnabled(bool enabled);
    void setPitchParams(bool enabled, float semitones);
    void setOctaveParams(bool enabled, float mix, float semitones);
    void setTuneParams(bool enabled, float correction, float speed);
    void setVocoderParams(bool enabled, float carrierFreq, float mix);

    float getCurrentAmplitude();
    oboe::DataCallbackResult onAudioReady(oboe::AudioStream *oboeStream, void *audioData, int32_t numFrames) override;

private:
    std::shared_ptr<oboe::AudioStream> recordingStream_;
    std::shared_ptr<oboe::AudioStream> playbackStream_;
    int32_t sampleRate_ = 48000;
    std::mutex dspMutex_;

    // Core Processing Instances
    soundtouch::SoundTouch soundTouch_;
    soundtouch::SoundTouch stOctave_;

    // --- TIME FX STATES ---
    std::atomic<bool> delayEnabled_{false}, reverbEnabled_{false};
    std::atomic<float> delayTimeL_{300.0f}, delayTimeR_{300.0f}, delayFeedback_{0.5f}, delayMix_{0.5f};
    std::atomic<float> reverbSize_{0.8f}, reverbDamping_{0.5f}, reverbMix_{0.3f};

    // --- MOD FX STATES ---
    std::atomic<bool> chorusEnabled_{false}, flangerEnabled_{false}, phaserEnabled_{false}, autoFilterEnabled_{false};
    std::atomic<float> chorusDelay_{7.5f}, chorusDepth_{6.5f}, chorusRate_{1.5f}, chorusMix_{0.5f};
    std::atomic<float> flangerDelay_{5.0f}, flangerDepth_{100.0f}, flangerRate_{0.6f}, flangerFeedback_{0.75f}, flangerMix_{0.5f};
    std::atomic<float> phaserRate_{1.0f}, phaserDepth_{0.8f}, phaserFeedback_{0.6f};
    std::atomic<float> afCutoff_{896.0f}, afResonance_{50.0f}, afRate_{1.0f}, afDepth_{31.0f};

    // --- TONE FX STATES ---
    std::atomic<bool> compEnabled_{false}, ampEnabled_{false}, eqEnabled_{false};
    std::atomic<float> compThresh_{-30.0f}, compRatio_{2.0f}, compAttack_{50.0f}, compRelease_{500.0f};
    std::atomic<float> ampDrive_{5.0f}, ampTone_{5000.0f}, ampOutput_{1.0f};
    std::atomic<bool> pitchEnabled_{false}, octaveEnabled_{false}, tuneEnabled_{false}, vocoderEnabled_{false};
    std::atomic<float> currentPitch_{0.0f}, octaveMix_{0.5f}, tuneCorrection_{1.0f}, vocoderFreq_{150.0f}, vocoderMix_{0.5f};

    // Internal Variables & Buffers
    float compEnvelope_ = 0.0f, ampToneY_ = 0.0f;
    float chorusPhase_ = 0.0f, flangerPhase_ = 0.0f, phaserPhase_ = 0.0f, afLfoPhase_ = 0.0f, vocoderPhase_ = 0.0f;
    BiquadState eqBands_[5];
    BiquadState autoFilterState_;

    std::vector<float> delayBufferL_, delayBufferR_, modBuffer_;
    int delayWriteIndex_ = 0, modWriteIndex_ = 0;
    std::vector<float> comb1_, comb2_, comb3_, comb4_, ap1_, ap2_;
    int cIdx1_=0, cIdx2_=0, cIdx3_=0, cIdx4_=0, aIdx1_=0, aIdx2_=0;

    std::atomic<float> currentAmplitude_{0.0f};
    std::atomic<bool> isRecording_{false};
    std::ofstream wavFile_;
    std::string currentWavPath_;
    uint32_t totalFramesWritten_ = 0;

    void processAudio(float* inputBuffer, float* outputBuffer, int32_t numFrames);
    void writeWavHeader();
    void updateWavHeader();
    void calculateBiquadCoeffs(BiquadState& state, float freq, float gainDb, float q, int type);
    float processBiquad(float input, BiquadState& state);
    float getFractionalDelay(std::vector<float>& buffer, int writeIndex, float delaySamples);
};

#endif //VOICECHANGER_AUDIOENGINE_H