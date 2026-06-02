#include "AudioEngine.h"
#include <android/log.h>
#include <cmath>
#include <algorithm>

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "AudioEngine", __VA_ARGS__)
const float PI = 3.14159265358979323846f;

// WAV Header structure
#pragma pack(push, 1)
struct WavHeader {
    char riff[4] = {'R','I','F','F'}; uint32_t chunkSize = 0; char wave[4] = {'W','A','V','E'};
    char fmt[4] = {'f','m','t',' '}; uint32_t fmtSize = 16; uint16_t audioFormat = 1;
    uint16_t numChannels = 2; uint32_t sampleRate = 48000; uint32_t byteRate = 48000 * 2 * sizeof(int16_t);
    uint16_t blockAlign = 2 * sizeof(int16_t); uint16_t bitsPerSample = 16;
    char data[4] = {'d','a','t','a'}; uint32_t dataSize = 0;
};
#pragma pack(pop)

AudioEngine::AudioEngine() {
    delayBufferL_.resize(48000 * 2, 0.0f); delayBufferR_.resize(48000 * 2, 0.0f); modBuffer_.resize(48000 * 2, 0.0f);
    comb1_.resize(1424, 0.0f); comb2_.resize(1781, 0.0f); comb3_.resize(1973, 0.0f); comb4_.resize(2076, 0.0f);
    ap1_.resize(240, 0.0f); ap2_.resize(82, 0.0f);

    // Init SoundTouch Main
    soundTouch_.setSampleRate(48000); soundTouch_.setChannels(1);
    soundTouch_.setTempoChange(0.0f); soundTouch_.setPitchSemiTones(0.0f);
    soundTouch_.setSetting(SETTING_USE_QUICKSEEK, 0); soundTouch_.setSetting(SETTING_USE_AA_FILTER, 1);

    // Init SoundTouch Octave
    stOctave_.setSampleRate(48000); stOctave_.setChannels(1);
    stOctave_.setTempoChange(0.0f); stOctave_.setPitchSemiTones(-12.0f);
    stOctave_.setSetting(SETTING_USE_QUICKSEEK, 0); stOctave_.setSetting(SETTING_USE_AA_FILTER, 1);

    // Default EQ initialization
    setEqBand(0, 0.0f, 100.0f, 0.707f); setEqBand(1, 0.0f, 400.0f, 1.0f);
    setEqBand(2, 0.0f, 1000.0f, 1.0f); setEqBand(3, 0.0f, 4000.0f, 1.0f); setEqBand(4, 0.0f, 10000.0f, 0.707f);
}

AudioEngine::~AudioEngine() { stop(); stopRecording(); }

void AudioEngine::calculateBiquadCoeffs(BiquadState& state, float freq, float gainDb, float q, int type) {
    float A = std::pow(10.0f, gainDb / 40.0f); float w0 = 2.0f * PI * freq / sampleRate_; float alpha = std::sin(w0) / (2.0f * q);
    float b0, b1, b2, a0, a1, a2;
    switch (type) {
        case 0: b0=A*((A+1.0f)-(A-1.0f)*std::cos(w0)+2.0f*std::sqrt(A)*alpha); b1=2.0f*A*((A-1.0f)-(A+1.0f)*std::cos(w0)); b2=A*((A+1.0f)-(A-1.0f)*std::cos(w0)-2.0f*std::sqrt(A)*alpha); a0=(A+1.0f)+(A-1.0f)*std::cos(w0)+2.0f*std::sqrt(A)*alpha; a1=-2.0f*((A-1.0f)+(A+1.0f)*std::cos(w0)); a2=(A+1.0f)+(A-1.0f)*std::cos(w0)-2.0f*std::sqrt(A)*alpha; break;
        case 1: b0=1.0f+alpha*A; b1=-2.0f*std::cos(w0); b2=1.0f-alpha*A; a0=1.0f+alpha/A; a1=-2.0f*std::cos(w0); a2=1.0f-alpha/A; break;
        case 2: b0=A*((A+1.0f)+(A-1.0f)*std::cos(w0)+2.0f*std::sqrt(A)*alpha); b1=-2.0f*A*((A-1.0f)+(A+1.0f)*std::cos(w0)); b2=A*((A+1.0f)+(A-1.0f)*std::cos(w0)-2.0f*std::sqrt(A)*alpha); a0=(A+1.0f)-(A-1.0f)*std::cos(w0)+2.0f*std::sqrt(A)*alpha; a1=2.0f*((A-1.0f)-(A+1.0f)*std::cos(w0)); a2=(A+1.0f)-(A-1.0f)*std::cos(w0)-2.0f*std::sqrt(A)*alpha; break;
        case 3: b0=(1.0f-std::cos(w0))/2.0f; b1=1.0f-std::cos(w0); b2=(1.0f-std::cos(w0))/2.0f; a0=1.0f+alpha; a1=-2.0f*std::cos(w0); a2=1.0f-alpha; break; // LowPass Filter
    }
    state.b0 = b0/a0; state.b1 = b1/a0; state.b2 = b2/a0; state.a1 = a1/a0; state.a2 = a2/a0;
}
float AudioEngine::processBiquad(float input, BiquadState& s) { float out = s.b0*input + s.b1*s.x1 + s.b2*s.x2 - s.a1*s.y1 - s.a2*s.y2; s.x2=s.x1; s.x1=input; s.y2=s.y1; s.y1=out; return out; }
float AudioEngine::getFractionalDelay(std::vector<float>& buffer, int writeIndex, float delaySamples) {
    float readPos = writeIndex - delaySamples; while (readPos < 0) readPos += buffer.size();
    int index1 = (int)readPos; int index2 = (index1 + 1) % buffer.size(); float frac = readPos - index1;
    return buffer[index1] + frac * (buffer[index2] - buffer[index1]);
}

// Parameter Setters
void AudioEngine::setDelayParams(bool e, float tl, float tr, float f, float m, bool pingPong) { delayEnabled_=e; delayTimeL_=tl; delayTimeR_=tr; delayFeedback_=f; delayMix_=m; delayPingPong_=pingPong; }
void AudioEngine::setReverbParams(bool e, float s, float d, float m) { reverbEnabled_=e; reverbSize_=s; reverbDamping_=d; reverbMix_=m; }
void AudioEngine::setChorusParams(bool e, float dMs, float dep, float r, float m) { chorusEnabled_=e; chorusDelay_=dMs; chorusDepth_=dep; chorusRate_=r; chorusMix_=m; }
void AudioEngine::setFlangerParams(bool e, float dMs, float dep, float r, float f, float m) { flangerEnabled_=e; flangerDelay_=dMs; flangerDepth_=dep; flangerRate_=r; flangerFeedback_=f; flangerMix_=m; }
void AudioEngine::setPhaserParams(bool e, float r, float d, float f) { phaserEnabled_=e; phaserRate_=r; phaserDepth_=d; phaserFeedback_=f; }
void AudioEngine::setAutoFilterParams(bool e, float c, float r, float lr, float ld) { autoFilterEnabled_=e; afCutoff_=c; afResonance_=r; afRate_=lr; afDepth_=ld; }
void AudioEngine::setCompParams(bool e, float t, float r, float a, float rel, float makeup) { compEnabled_=e; compThresh_=t; compRatio_=r; compAttack_=a; compRelease_=rel; compMakeup_=makeup; }
void AudioEngine::setAmpParams(bool e, float d, float t, float o) { ampEnabled_=e; ampDrive_=d; ampTone_=t; ampOutput_=o; }
void AudioEngine::setEqBand(int b, float g, float f, float q) { int t=1; if(b==0) t=0; if(b==4) t=2; calculateBiquadCoeffs(eqBands_[b], f, g, q, t); }
void AudioEngine::setEqEnabled(bool e) { eqEnabled_=e; }
void AudioEngine::setPitchParams(bool e, float s) { std::lock_guard<std::mutex> lock(dspMutex_); pitchEnabled_=e; currentPitch_=s; soundTouch_.setPitchSemiTones(s); }
void AudioEngine::setOctaveParams(bool e, float m, float s) { std::lock_guard<std::mutex> lock(dspMutex_); octaveEnabled_=e; octaveMix_=m; stOctave_.setPitchSemiTones(s); }
void AudioEngine::setTuneParams(bool e, float c, float s) { tuneEnabled_=e; tuneCorrection_=c; }
void AudioEngine::setVocoderParams(bool e, float c, float m) { vocoderEnabled_=e; vocoderFreq_=c; vocoderMix_=m; }
float AudioEngine::getCurrentAmplitude() { return currentAmplitude_.load(); }

// Live Recording
void AudioEngine::startRecording(const char* path) { if(isRecording_) stopRecording(); currentWavPath_=path; wavFile_.open(currentWavPath_, std::ios::binary); if(wavFile_.is_open()) { totalFramesWritten_=0; writeWavHeader(); isRecording_=true; } }
void AudioEngine::stopRecording() { if(isRecording_ && wavFile_.is_open()) { isRecording_=false; updateWavHeader(); wavFile_.close(); } }
void AudioEngine::writeWavHeader() { WavHeader h; wavFile_.seekp(0, std::ios::beg); wavFile_.write(reinterpret_cast<const char*>(&h), sizeof(WavHeader)); }
void AudioEngine::updateWavHeader() { WavHeader h; h.dataSize = totalFramesWritten_*2*sizeof(int16_t); h.chunkSize = 36+h.dataSize; wavFile_.seekp(0, std::ios::beg); wavFile_.write(reinterpret_cast<const char*>(&h), sizeof(WavHeader)); }

// Offline Playback and Processing
void AudioEngine::startPlayback(const char* filePath) {
    std::lock_guard<std::mutex> lock(dspMutex_);
    std::ifstream inFile(filePath, std::ios::binary);
    if(!inFile) return;
    WavHeader header;
    inFile.read((char*)&header, sizeof(WavHeader));

    inFile.seekg(0, std::ios::end);
    size_t fileSize = inFile.tellg();
    inFile.seekg(sizeof(WavHeader), std::ios::beg);
    size_t dataBytes = fileSize - sizeof(WavHeader);

    std::vector<int16_t> inData(dataBytes / sizeof(int16_t));
    inFile.read((char*)inData.data(), dataBytes);
    inFile.close();

    playbackBuffer_.clear();
    // Convert Stereo 16-bit PCM to Mono Float [-1.0, 1.0] for our Engine
    for(size_t i=0; i<inData.size(); i+=2) {
        float l = inData[i] / 32768.0f;
        float r = (i+1 < inData.size()) ? inData[i+1] / 32768.0f : l;
        playbackBuffer_.push_back((l+r)*0.5f);
    }
    playbackIndex_ = 0;
    isPlaying_ = true;
}

void AudioEngine::stopPlayback() { isPlaying_ = false; }

float AudioEngine::getPlaybackPosition() {
    if(playbackBuffer_.empty() || !isPlaying_.load()) return 0.0f;
    return (float)playbackIndex_.load() / playbackBuffer_.size();
}

bool AudioEngine::processWavFile(const char* inPath, const char* outPath) {
    std::lock_guard<std::mutex> lock(dspMutex_);

    std::ifstream inFile(inPath, std::ios::binary);
    if(!inFile) return false;
    WavHeader header;
    inFile.read((char*)&header, sizeof(WavHeader));
    inFile.seekg(0, std::ios::end);
    size_t fileSize = inFile.tellg();
    inFile.seekg(sizeof(WavHeader), std::ios::beg);
    size_t dataBytes = fileSize - sizeof(WavHeader);
    std::vector<int16_t> inData(dataBytes / sizeof(int16_t));
    inFile.read((char*)inData.data(), dataBytes);
    inFile.close();

    std::vector<float> monoBuffer;
    for(size_t i=0; i<inData.size(); i+=2) {
        float l = inData[i] / 32768.0f;
        float r = (i+1 < inData.size()) ? inData[i+1] / 32768.0f : l;
        monoBuffer.push_back((l+r)*0.5f);
    }

    std::vector<int16_t> outData;
    int chunkSize = 1024;
    std::vector<float> chunkOut(chunkSize * 2);

    // Reset DSP buffers for clean render
    soundTouch_.clear(); stOctave_.clear();
    std::fill(delayBufferL_.begin(), delayBufferL_.end(), 0.0f);
    std::fill(delayBufferR_.begin(), delayBufferR_.end(), 0.0f);
    std::fill(modBuffer_.begin(), modBuffer_.end(), 0.0f);

    for(size_t i=0; i<monoBuffer.size(); i+=chunkSize) {
        int frames = std::min(chunkSize, (int)(monoBuffer.size() - i));
        coreDSP(&monoBuffer[i], chunkOut.data(), frames);
        for(int j=0; j<frames*2; j++) outData.push_back((int16_t)(chunkOut[j] * 32767.0f));
    }

    std::ofstream outFile(outPath, std::ios::binary);
    if(!outFile) return false;
    header.dataSize = outData.size() * sizeof(int16_t);
    header.chunkSize = 36 + header.dataSize;
    outFile.write((char*)&header, sizeof(WavHeader));
    outFile.write((char*)outData.data(), header.dataSize);
    outFile.close();

    return true;
}

// Real-Time Oboe Integration
bool AudioEngine::start() {
    oboe::AudioStreamBuilder b;
    b.setDirection(oboe::Direction::Input)->setPerformanceMode(oboe::PerformanceMode::LowLatency)->setFormat(oboe::AudioFormat::Float)->setChannelCount(1);
    if (b.openStream(recordingStream_) != oboe::Result::OK) return false;
    sampleRate_ = recordingStream_->getSampleRate();

    b.setDirection(oboe::Direction::Output)->setDataCallback(this)->setFormat(oboe::AudioFormat::Float)->setChannelCount(2);
    if (b.openStream(playbackStream_) != oboe::Result::OK) return false;

    playbackStream_->requestStart(); recordingStream_->requestStart(); return true;
}

void AudioEngine::stop() {
    if (recordingStream_) { recordingStream_->requestStop(); recordingStream_->close(); recordingStream_.reset(); }
    if (playbackStream_) { playbackStream_->requestStop(); playbackStream_->close(); playbackStream_.reset(); }
}

oboe::DataCallbackResult AudioEngine::onAudioReady(oboe::AudioStream *stream, void *audioData, int32_t numFrames) {
    auto *outBuf = static_cast<float *>(audioData); float inBuf[numFrames];

    if (isPlaying_.load()) {
        // Read from file buffer if playing back
        for(int i=0; i<numFrames; i++) {
            size_t idx = playbackIndex_.load();
            if (idx < playbackBuffer_.size()) {
                inBuf[i] = playbackBuffer_[idx];
                playbackIndex_.store(idx + 1);
            } else {
                inBuf[i] = 0.0f;
                isPlaying_ = false; // End of File
            }
        }
    } else {
        // Read from mic if recording/live monitoring
        auto result = recordingStream_->read(inBuf, numFrames, 0);
        if (!result || result.value() == 0) { for(int i=0; i<numFrames*2; i++) outBuf[i] = 0.0f; return oboe::DataCallbackResult::Continue; }
    }

    processAudio(inBuf, outBuf, numFrames);
    return oboe::DataCallbackResult::Continue;
}

void AudioEngine::processAudio(float* inputBuffer, float* outputBuffer, int32_t numFrames) {
    std::lock_guard<std::mutex> lock(dspMutex_);
    coreDSP(inputBuffer, outputBuffer, numFrames);

    // Write live output to WAV if recording
    if (isRecording_ && wavFile_.is_open()) {
        std::vector<int16_t> wavData(numFrames * 2);
        for(int i=0; i<numFrames*2; i++) wavData[i] = (int16_t)(outputBuffer[i]*32767.f);
        wavFile_.write(reinterpret_cast<const char*>(wavData.data()), wavData.size()*sizeof(int16_t));
        totalFramesWritten_ += numFrames;
    }
}

void AudioEngine::coreDSP(float* inputBuffer, float* outputBuffer, int32_t numFrames) {
    float peakAmp = 0.0f;
    std::vector<float> pBuf(inputBuffer, inputBuffer + numFrames);
    std::vector<float> octBuf(numFrames, 0.0f);

    // Pitch Shifter
    if (pitchEnabled_.load()) {
        soundTouch_.putSamples(pBuf.data(), numFrames);
        int ready = soundTouch_.numSamples();
        if (ready >= numFrames) soundTouch_.receiveSamples(pBuf.data(), numFrames);
        else { soundTouch_.receiveSamples(pBuf.data(), ready); for(int i=ready; i<numFrames; i++) pBuf[i]=0.0f; }
    } else { soundTouch_.clear(); }

    // Octave generator
    if (octaveEnabled_.load()) {
        stOctave_.putSamples(inputBuffer, numFrames);
        int ready = stOctave_.numSamples();
        if (ready >= numFrames) stOctave_.receiveSamples(octBuf.data(), numFrames);
        else { stOctave_.receiveSamples(octBuf.data(), ready); for(int i=ready; i<numFrames; i++) octBuf[i]=0.0f; }
    } else { stOctave_.clear(); }

    for (int i = 0; i < numFrames; i++) {
        float sample = pBuf[i];
        if (octaveEnabled_.load()) sample = (sample * (1.0f - octaveMix_.load())) + (octBuf[i] * octaveMix_.load());
        if (std::abs(sample) > peakAmp) peakAmp = std::abs(sample);

        // Auto Tune Proxy
        if (tuneEnabled_.load()) {
            int shift = (int)(tuneCorrection_.load() * 500.0f); int dS = (modWriteIndex_ * 2) % std::max(5, shift);
            sample = (sample + getFractionalDelay(modBuffer_, modWriteIndex_, dS)) * 0.5f;
        }

        // Compressor with Makeup Gain
        if (compEnabled_.load()) {
            float envInput = std::abs(sample);
            if(envInput > compEnvelope_) compEnvelope_ += (envInput - compEnvelope_) * (1.0f - std::exp(-1.0f / (compAttack_.load() * 0.001f * sampleRate_)));
            else compEnvelope_ += (envInput - compEnvelope_) * (1.0f - std::exp(-1.0f / (compRelease_.load() * 0.001f * sampleRate_)));
            float envDb = 20.0f * std::log10(std::max(compEnvelope_, 1e-6f));
            if(envDb > compThresh_.load()) {
                float gainReductionDb = (compThresh_.load() - envDb) * (1.0f - 1.0f / compRatio_.load());
                sample *= std::pow(10.0f, gainReductionDb / 20.0f);
            }
            // Apply Makeup Gain
            sample *= std::pow(10.0f, compMakeup_.load() / 20.0f);
        }

        // Vocoder Proxy
        if (vocoderEnabled_.load()) {
            vocoderPhase_ += (vocoderFreq_.load() * 2.0f * PI) / sampleRate_;
            if (vocoderPhase_ > 2.0f * PI) vocoderPhase_ -= 2.0f * PI;
            float carrier = std::sin(vocoderPhase_);
            float wet = sample * carrier * 2.0f;
            sample = (sample * (1.0f - vocoderMix_.load())) + (wet * vocoderMix_.load());
        }

        // EQ Bands
        if (eqEnabled_.load()) { for(int b=0; b<5; b++) sample = processBiquad(sample, eqBands_[b]); }

        // Auto Filter
        if (autoFilterEnabled_.load()) {
            afLfoPhase_ += (afRate_.load() * 2.0f * PI) / sampleRate_;
            if (afLfoPhase_ > 2.0f * PI) afLfoPhase_ -= 2.0f * PI;
            float cutoff = afCutoff_.load() + (std::sin(afLfoPhase_) * (afDepth_.load() / 100.0f) * afCutoff_.load());
            cutoff = std::clamp(cutoff, 20.0f, 20000.0f);
            calculateBiquadCoeffs(autoFilterState_, cutoff, 0.0f, afResonance_.load() / 100.0f + 0.1f, 3);
            sample = processBiquad(sample, autoFilterState_);
        }

        // Amplifier
        if (ampEnabled_.load()) {
            sample = std::tanh(sample * ampDrive_.load());
            float rc = 1.0f / (2.0f * PI * ampTone_.load()); float alpha = (1.0f/sampleRate_) / (rc + (1.0f/sampleRate_));
            ampToneY_ = ampToneY_ + alpha * (sample - ampToneY_); sample = ampToneY_ * ampOutput_.load();
        }

        modBuffer_[modWriteIndex_] = sample;

        // Chorus
        if (chorusEnabled_.load()) {
            chorusPhase_ += (chorusRate_.load() * 2.0f * PI) / sampleRate_; if(chorusPhase_ > 2.0f * PI) chorusPhase_ -= 2.0f * PI;
            float lfo = (std::sin(chorusPhase_) + 1.0f) * 0.5f * (chorusDepth_.load() / 1000.0f) * sampleRate_;
            float delayed = getFractionalDelay(modBuffer_, modWriteIndex_, lfo + (chorusDelay_.load() / 1000.0f * sampleRate_));
            sample = (sample * (1.0f - chorusMix_.load())) + (delayed * chorusMix_.load());
        }

        // Flanger
        if (flangerEnabled_.load()) {
            flangerPhase_ += (flangerRate_.load() * 2.0f * PI) / sampleRate_; if(flangerPhase_ > 2.0f * PI) flangerPhase_ -= 2.0f * PI;
            float lfo = (std::sin(flangerPhase_) + 1.0f) * 0.5f * (flangerDepth_.load() / 100.0f * 5.0f / 1000.0f) * sampleRate_;
            float delayed = getFractionalDelay(modBuffer_, modWriteIndex_, lfo + (flangerDelay_.load() / 1000.0f * sampleRate_));
            sample = (sample * (1.0f - flangerMix_.load())) + (delayed * flangerMix_.load());
            modBuffer_[modWriteIndex_] = sample + (delayed * flangerFeedback_.load());
        }

        // Phaser Proxy
        if (phaserEnabled_.load()) {
            phaserPhase_ += (phaserRate_.load() * 2.0f * PI) / sampleRate_; if(phaserPhase_ > 2.0f * PI) phaserPhase_ -= 2.0f * PI;
            float lfo = (std::sin(phaserPhase_) + 1.0f) * 0.5f * phaserDepth_.load() * 20.0f;
            sample = (sample * 0.7f) + (getFractionalDelay(modBuffer_, modWriteIndex_, lfo) * phaserFeedback_.load());
        }

        float outL = sample; float outR = sample;

        // Stereo Delay with Ping Pong Option
        if (delayEnabled_.load()) {
            float dL = getFractionalDelay(delayBufferL_, delayWriteIndex_, sampleRate_ * delayTimeL_.load() / 1000.0f);
            float dR = getFractionalDelay(delayBufferR_, delayWriteIndex_, sampleRate_ * delayTimeR_.load() / 1000.0f);
            outL += dL * delayMix_.load(); outR += dR * delayMix_.load();

            if (delayPingPong_.load()) {
                // Cross feedback for Ping Pong
                delayBufferL_[delayWriteIndex_] = sample + (dR * delayFeedback_.load());
                delayBufferR_[delayWriteIndex_] = sample + (dL * delayFeedback_.load());
            } else {
                // Normal feedback
                delayBufferL_[delayWriteIndex_] = sample + (dL * delayFeedback_.load());
                delayBufferR_[delayWriteIndex_] = sample + (dR * delayFeedback_.load());
            }
        } else { delayBufferL_[delayWriteIndex_] = sample; delayBufferR_[delayWriteIndex_] = sample; }

        // Reverb
        if (reverbEnabled_.load()) {
            float revIn = (outL + outR) * 0.5f; float f = reverbSize_.load(); float d = reverbDamping_.load();
            float outC1 = comb1_[cIdx1_]; comb1_[cIdx1_] = revIn + (outC1*f*(1.0f-d)); cIdx1_=(cIdx1_+1)%comb1_.size();
            float outC2 = comb2_[cIdx2_]; comb2_[cIdx2_] = revIn + (outC2*f*(1.0f-d)); cIdx2_=(cIdx2_+1)%comb2_.size();
            float outC3 = comb3_[cIdx3_]; comb3_[cIdx3_] = revIn + (outC3*f*(1.0f-d)); cIdx3_=(cIdx3_+1)%comb3_.size();
            float outC4 = comb4_[cIdx4_]; comb4_[cIdx4_] = revIn + (outC4*f*(1.0f-d)); cIdx4_=(cIdx4_+1)%comb4_.size();
            float cSum = (outC1+outC2+outC3+outC4)*0.25f; float apG = 0.7f;
            float apOut1 = -apG*cSum+ap1_[aIdx1_]; ap1_[aIdx1_]=cSum+apG*ap1_[aIdx1_]; aIdx1_=(aIdx1_+1)%ap1_.size();
            float apOut2 = -apG*apOut1+ap2_[aIdx2_]; ap2_[aIdx2_]=apOut1+apG*ap2_[aIdx2_]; aIdx2_=(aIdx2_+1)%ap2_.size();
            outL += apOut2 * reverbMix_.load(); outR += apOut2 * reverbMix_.load();
        }

        // Limit Output
        outL = std::clamp(outL, -1.0f, 1.0f); outR = std::clamp(outR, -1.0f, 1.0f);
        outputBuffer[i*2] = outL; outputBuffer[i*2+1] = outR;

        delayWriteIndex_ = (delayWriteIndex_+1)%delayBufferL_.size(); modWriteIndex_ = (modWriteIndex_+1)%modBuffer_.size();
    }
    currentAmplitude_.store(peakAmp);
}