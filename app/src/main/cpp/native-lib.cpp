#include <jni.h>
#include "AudioEngine.h"

// Global instance of the audio engine
AudioEngine* audioEngine = nullptr;

extern "C" JNIEXPORT jboolean JNICALL Java_com_magics_voice_changer_VoiceEngine_startEngine(JNIEnv *env, jobject thiz) {
    if (!audioEngine) audioEngine = new AudioEngine();
    return audioEngine->start() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL Java_com_magics_voice_changer_VoiceEngine_stopEngine(JNIEnv *env, jobject thiz) {
    if (audioEngine) {
        audioEngine->stop();
        delete audioEngine;
        audioEngine = nullptr;
    }
}

extern "C" JNIEXPORT void JNICALL Java_com_magics_voice_changer_VoiceEngine_setPitchParams(JNIEnv *env, jobject thiz, jboolean enabled, jfloat semitones) {
    if (audioEngine) audioEngine->setPitchParams(enabled, semitones);
}

extern "C" JNIEXPORT void JNICALL Java_com_magics_voice_changer_VoiceEngine_setChorusParams(JNIEnv *env, jobject thiz, jboolean enabled, jfloat depth, jfloat freq) {
    if (audioEngine) audioEngine->setChorusParams(enabled, depth, freq);
}

extern "C" JNIEXPORT void JNICALL Java_com_magics_voice_changer_VoiceEngine_setDelayParams(JNIEnv *env, jobject thiz, jboolean enabled, jfloat delayMs, jfloat feedback) {
    if (audioEngine) audioEngine->setDelayParams(enabled, delayMs, feedback);
}

extern "C" JNIEXPORT jfloat JNICALL Java_com_magics_voice_changer_VoiceEngine_getAmplitude(JNIEnv *env, jobject thiz) {
    return audioEngine ? audioEngine->getCurrentAmplitude() : 0.0f;
}

extern "C" JNIEXPORT void JNICALL Java_com_magics_voice_changer_VoiceEngine_startRecording(JNIEnv *env, jobject thiz, jstring path) {
    if (!audioEngine) return;
    const char *filePath = env->GetStringUTFChars(path, 0);
    audioEngine->startRecording(filePath);
    env->ReleaseStringUTFChars(path, filePath);
}

extern "C" JNIEXPORT void JNICALL Java_com_magics_voice_changer_VoiceEngine_stopRecording(JNIEnv *env, jobject thiz) {
    if (audioEngine) {
        audioEngine->stopRecording();
    }
}