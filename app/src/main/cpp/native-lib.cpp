#include <jni.h>
#include "AudioEngine.h"

// Global instance of the audio engine
AudioEngine* audioEngine = nullptr;

extern "C" JNIEXPORT jboolean JNICALL
Java_com_magics_voice_changer_VoiceEngine_startEngine(JNIEnv *env, jobject thiz) {
    if (!audioEngine) {
        audioEngine = new AudioEngine();
    }
    return audioEngine->start() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_magics_voice_changer_VoiceEngine_stopEngine(JNIEnv *env, jobject thiz) {
    if (audioEngine) {
        audioEngine->stop();
        delete audioEngine;
        audioEngine = nullptr;
    }
}

// 5-Band Pro EQ JNI Bridge
extern "C" JNIEXPORT void JNICALL
Java_com_magics_voice_changer_VoiceEngine_setEqBand(JNIEnv *env, jobject thiz, jint band_index, jfloat gain_db, jfloat freq_hz, jfloat q) {
    if (audioEngine) {
        audioEngine->setEqBand(band_index, gain_db, freq_hz, q);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_magics_voice_changer_VoiceEngine_setEqEnabled(JNIEnv *env, jobject thiz, jboolean enabled) {
    if (audioEngine) {
        audioEngine->setEqEnabled(enabled);
    }
}

// Morpher JNI Bridge
extern "C" JNIEXPORT void JNICALL
Java_com_magics_voice_changer_VoiceEngine_setMorphParams(JNIEnv *env, jobject thiz, jboolean enabled, jfloat pitch_shift) {
    if (audioEngine) {
        audioEngine->setMorphParams(enabled, pitch_shift);
    }
}

// Stereo Delay JNI Bridge
extern "C" JNIEXPORT void JNICALL
Java_com_magics_voice_changer_VoiceEngine_setDelayParams(JNIEnv *env, jobject thiz, jboolean enabled, jfloat time_l, jfloat time_r, jfloat feedback, jfloat mix) {
    if (audioEngine) {
        audioEngine->setDelayParams(enabled, time_l, time_r, feedback, mix);
    }
}

// Schroeder Reverb JNI Bridge
extern "C" JNIEXPORT void JNICALL
Java_com_magics_voice_changer_VoiceEngine_setReverbParams(JNIEnv *env, jobject thiz, jboolean enabled, jfloat size, jfloat damping, jfloat mix) {
    if (audioEngine) {
        audioEngine->setReverbParams(enabled, size, damping, mix);
    }
}

// UI Feedback JNI Bridge
extern "C" JNIEXPORT jfloat JNICALL
Java_com_magics_voice_changer_VoiceEngine_getAmplitude(JNIEnv *env, jobject thiz) {
    return audioEngine ? audioEngine->getCurrentAmplitude() : 0.0f;
}

// Recording JNI Bridge
extern "C" JNIEXPORT void JNICALL
Java_com_magics_voice_changer_VoiceEngine_startRecording(JNIEnv *env, jobject thiz, jstring path) {
    if (!audioEngine) return;
    const char *filePath = env->GetStringUTFChars(path, 0);
    audioEngine->startRecording(filePath);
    env->ReleaseStringUTFChars(path, filePath);
}

extern "C" JNIEXPORT void JNICALL
Java_com_magics_voice_changer_VoiceEngine_stopRecording(JNIEnv *env, jobject thiz) {
    if (audioEngine) {
        audioEngine->stopRecording();
    }
}