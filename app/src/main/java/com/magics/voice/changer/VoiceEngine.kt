package com.magics.voice.changer

object VoiceEngine {
    init {
        // Load the C++ native library
        System.loadLibrary("voicechanger")
    }

    external fun startEngine(): Boolean
    external fun stopEngine()

    // SoundTouch Parameters
    external fun setPitchParams(enabled: Boolean, semitones: Float)

    // STK Parameters
    external fun setChorusParams(enabled: Boolean, depth: Float, freq: Float)
    external fun setDelayParams(enabled: Boolean, delayMs: Float, feedback: Float)

    // UI Visualization
    external fun getAmplitude(): Float

    // WAV Recording
    external fun startRecording(path: String)
    external fun stopRecording()
}