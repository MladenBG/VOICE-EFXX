package com.magics.voice.changer

object VoiceEngine {
    init {
        // Load the C++ native library
        System.loadLibrary("voicechanger")
    }

    external fun startEngine(): Boolean
    external fun stopEngine()

    // EQ Controls
    external fun setEqBand(bandIndex: Int, gainDb: Float, freqHz: Float, q: Float)
    external fun setEqEnabled(enabled: Boolean)

    // Effect Controls
    external fun setMorphParams(enabled: Boolean, pitchShift: Float)
    external fun setDelayParams(enabled: Boolean, timeL: Float, timeR: Float, feedback: Float, mix: Float)
    external fun setReverbParams(enabled: Boolean, size: Float, damping: Float, mix: Float)

    // UI Visualization
    external fun getAmplitude(): Float

    // WAV Recording
    external fun startRecording(path: String)
    external fun stopRecording()
}