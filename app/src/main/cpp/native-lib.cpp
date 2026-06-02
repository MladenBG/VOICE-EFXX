#include <jni.h>
#include "AudioEngine.h"

AudioEngine* audioEngine = nullptr;

extern "C" JNIEXPORT jboolean JNICALL Java_com_magics_voice_changer_VoiceEngine_startEngine(JNIEnv *e, jobject t) {
    if (!audioEngine) audioEngine = new AudioEngine(); return audioEngine->start() ? JNI_TRUE : JNI_FALSE;
}
extern "C" JNIEXPORT void JNICALL Java_com_magics_voice_changer_VoiceEngine_stopEngine(JNIEnv *e, jobject t) {
    if (audioEngine) { audioEngine->stop(); delete audioEngine; audioEngine = nullptr; }
}

// OFFLINE & PLAYBACK JNI
extern "C" JNIEXPORT void JNICALL Java_com_magics_voice_changer_VoiceEngine_startPlayback(JNIEnv *e, jobject t, jstring p) {
    if (!audioEngine) return; const char *f = e->GetStringUTFChars(p, 0); audioEngine->startPlayback(f); e->ReleaseStringUTFChars(p, f);
}
extern "C" JNIEXPORT void JNICALL Java_com_magics_voice_changer_VoiceEngine_stopPlayback(JNIEnv *e, jobject t) {
    if (audioEngine) audioEngine->stopPlayback();
}
extern "C" JNIEXPORT jfloat JNICALL Java_com_magics_voice_changer_VoiceEngine_getPlaybackPosition(JNIEnv *e, jobject t) {
    return audioEngine ? audioEngine->getPlaybackPosition() : 0.0f;
}
extern "C" JNIEXPORT jboolean JNICALL Java_com_magics_voice_changer_VoiceEngine_processWavFile(JNIEnv *e, jobject t, jstring in, jstring out) {
    if (!audioEngine) return JNI_FALSE;
    const char *inFile = e->GetStringUTFChars(in, 0); const char *outFile = e->GetStringUTFChars(out, 0);
    bool res = audioEngine->processWavFile(inFile, outFile);
    e->ReleaseStringUTFChars(in, inFile); e->ReleaseStringUTFChars(out, outFile);
    return res ? JNI_TRUE : JNI_FALSE;
}

// Tone FX JNI (Dodat MAKEUP parametar ovde)
extern "C" JNIEXPORT void JNICALL Java_com_magics_voice_changer_VoiceEngine_setCompParams(JNIEnv *e, jobject t, jboolean en, jfloat thr, jfloat rat, jfloat att, jfloat rel, jfloat mak) {
    if (audioEngine) audioEngine->setCompParams(en, thr, rat, att, rel, mak);
}
extern "C" JNIEXPORT void JNICALL Java_com_magics_voice_changer_VoiceEngine_setAmpParams(JNIEnv *e, jobject t, jboolean en, jfloat d, jfloat tn, jfloat o) {
    if (audioEngine) audioEngine->setAmpParams(en, d, tn, o);
}
extern "C" JNIEXPORT void JNICALL Java_com_magics_voice_changer_VoiceEngine_setEqBand(JNIEnv *e, jobject t, jint b, jfloat g, jfloat f, jfloat q) {
    if (audioEngine) audioEngine->setEqBand(b, g, f, q);
}
extern "C" JNIEXPORT void JNICALL Java_com_magics_voice_changer_VoiceEngine_setEqEnabled(JNIEnv *e, jobject t, jboolean en) {
    if (audioEngine) audioEngine->setEqEnabled(en);
}
extern "C" JNIEXPORT void JNICALL Java_com_magics_voice_changer_VoiceEngine_setPitchParams(JNIEnv *e, jobject t, jboolean en, jfloat s) {
    if (audioEngine) audioEngine->setPitchParams(en, s);
}
extern "C" JNIEXPORT void JNICALL Java_com_magics_voice_changer_VoiceEngine_setOctaveParams(JNIEnv *e, jobject t, jboolean en, jfloat m, jfloat s) {
    if (audioEngine) audioEngine->setOctaveParams(en, m, s);
}
extern "C" JNIEXPORT void JNICALL Java_com_magics_voice_changer_VoiceEngine_setTuneParams(JNIEnv *e, jobject t, jboolean en, jfloat c, jfloat s) {
    if (audioEngine) audioEngine->setTuneParams(en, c, s);
}
extern "C" JNIEXPORT void JNICALL Java_com_magics_voice_changer_VoiceEngine_setVocoderParams(JNIEnv *e, jobject t, jboolean en, jfloat c, jfloat m) {
    if (audioEngine) audioEngine->setVocoderParams(en, c, m);
}

// Mod FX JNI
extern "C" JNIEXPORT void JNICALL Java_com_magics_voice_changer_VoiceEngine_setChorusParams(JNIEnv *e, jobject t, jboolean en, jfloat dms, jfloat d, jfloat r, jfloat m) {
    if (audioEngine) audioEngine->setChorusParams(en, dms, d, r, m);
}
extern "C" JNIEXPORT void JNICALL Java_com_magics_voice_changer_VoiceEngine_setFlangerParams(JNIEnv *e, jobject t, jboolean en, jfloat dms, jfloat d, jfloat r, jfloat fb, jfloat m) {
    if (audioEngine) audioEngine->setFlangerParams(en, dms, d, r, fb, m);
}
extern "C" JNIEXPORT void JNICALL Java_com_magics_voice_changer_VoiceEngine_setPhaserParams(JNIEnv *e, jobject t, jboolean en, jfloat r, jfloat d, jfloat fb) {
    if (audioEngine) audioEngine->setPhaserParams(en, r, d, fb);
}
extern "C" JNIEXPORT void JNICALL Java_com_magics_voice_changer_VoiceEngine_setAutoFilterParams(JNIEnv *e, jobject t, jboolean en, jfloat c, jfloat res, jfloat lr, jfloat ld) {
    if (audioEngine) audioEngine->setAutoFilterParams(en, c, res, lr, ld);
}

// Time FX JNI (Dodat PING-PONG parametar ovde)
extern "C" JNIEXPORT void JNICALL Java_com_magics_voice_changer_VoiceEngine_setDelayParams(JNIEnv *e, jobject t, jboolean en, jfloat tl, jfloat tr, jfloat f, jfloat m, jboolean pp) {
    if (audioEngine) audioEngine->setDelayParams(en, tl, tr, f, m, pp);
}
extern "C" JNIEXPORT void JNICALL Java_com_magics_voice_changer_VoiceEngine_setReverbParams(JNIEnv *e, jobject t, jboolean en, jfloat s, jfloat d, jfloat m) {
    if (audioEngine) audioEngine->setReverbParams(en, s, d, m);
}

// System JNI
extern "C" JNIEXPORT jfloat JNICALL Java_com_magics_voice_changer_VoiceEngine_getAmplitude(JNIEnv *e, jobject t) {
    return audioEngine ? audioEngine->getCurrentAmplitude() : 0.0f;
}
extern "C" JNIEXPORT void JNICALL Java_com_magics_voice_changer_VoiceEngine_startRecording(JNIEnv *e, jobject t, jstring p) {
    if (!audioEngine) return; const char *f = e->GetStringUTFChars(p, 0); audioEngine->startRecording(f); e->ReleaseStringUTFChars(p, f);
}
extern "C" JNIEXPORT void JNICALL Java_com_magics_voice_changer_VoiceEngine_stopRecording(JNIEnv *e, jobject t) {
    if (audioEngine) audioEngine->stopRecording();
}