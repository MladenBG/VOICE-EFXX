#ifndef VOICECHANGER_AUDIOENGINE_H
#define VOICECHANGER_AUDIOENGINE_H

#include <oboe/Oboe.h>
#include <atomic>
#include <mutex>
#include <vector>
#include <string>
#include <fstream>

// Official headers from the libraries
#include "SoundTouch.h"
#include "Stk.h"
#include "Chorus.h"
#include "Echo.h"

class AudioEngine : public oboe::AudioStreamDataCallback {
public:
    AudioEngine();
    ~AudioEngine();

    bool start();
    void stop();

    // WAV Recording
    void startRecording(const char* filePath);
    void stopRecording();

    // Parameters for SoundTouch
    void setPitchParams(bool enabled, float pitchSemitones);

    // Parameters for STK
    void setChorusParams(bool enabled, float modDepth, float modFrequency);
    void setDelayParams(bool enabled, float delayMs, float feedback);

    float getCurrentAmplitude();

    oboe::DataCallbackResult onAudioReady(oboe::AudioStream *oboeStream, void *audioData, int32_t numFrames) override;

private:
    std::shared_ptr<oboe::AudioStream> recordingStream_;
    std::shared_ptr<oboe::AudioStream> playbackStream_;
    int32_t sampleRate_ = 48000;

    // Instances of professional effects
    soundtouch::SoundTouch soundTouch_;
    stk::Chorus stkChorus_;
    stk::Echo stkDelay_;

    // States
    std::atomic<bool> pitchEnabled_{false};
    std::atomic<float> currentPitch_{0.0f};

    std::atomic<bool> chorusEnabled_{false};
    std::atomic<float> chorusDepth_{0.2f};
    std::atomic<float> chorusFreq_{1.5f};

    std::atomic<bool> delayEnabled_{false};
    std::atomic<float> delayTime_{300.0f};
    std::atomic<float> delayFeedback_{0.5f};

    std::atomic<float> currentAmplitude_{0.0f};
    std::mutex dspMutex_;

    // Recording State
    std::atomic<bool> isRecording_{false};
    std::ofstream wavFile_;
    std::string currentWavPath_;
    uint32_t totalFramesWritten_ = 0;

    void processAudio(float* inputBuffer, float* outputBuffer, int32_t numFrames);
    void writeWavHeader();
    void updateWavHeader();
};

#endif //VOICECHANGER_AUDIOENGINE_H