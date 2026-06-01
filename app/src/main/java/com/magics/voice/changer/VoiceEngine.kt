package com.magics.voice.changer

object VoiceEngine {
    init { System.loadLibrary("voicechanger") }

    external fun startEngine(): Boolean
    external fun stopEngine()

    // Tone FX
    external fun setCompParams(enabled: Boolean, thresh: Float, ratio: Float, attack: Float, release: Float)
    external fun setAmpParams(enabled: Boolean, drive: Float, tone: Float, output: Float)
    external fun setEqBand(bandIndex: Int, gainDb: Float, freqHz: Float, q: Float)
    external fun setEqEnabled(enabled: Boolean)
    external fun setPitchParams(enabled: Boolean, semitones: Float)
    external fun setOctaveParams(enabled: Boolean, mix: Float, semitones: Float)
    external fun setTuneParams(enabled: Boolean, correction: Float, speed: Float)
    external fun setVocoderParams(enabled: Boolean, carrierFreq: Float, mix: Float)

    // Mod FX
    external fun setChorusParams(enabled: Boolean, delayMs: Float, depth: Float, rate: Float, mix: Float)
    external fun setFlangerParams(enabled: Boolean, delayMs: Float, depth: Float, rate: Float, feedback: Float, mix: Float)
    external fun setPhaserParams(enabled: Boolean, rate: Float, depth: Float, feedback: Float)
    external fun setAutoFilterParams(enabled: Boolean, cutoff: Float, res: Float, lfoRate: Float, lfoDepth: Float)

    // Time FX
    external fun setDelayParams(enabled: Boolean, timeL: Float, timeR: Float, feedback: Float, mix: Float)
    external fun setReverbParams(enabled: Boolean, size: Float, damping: Float, mix: Float)

    external fun getAmplitude(): Float
    external fun startRecording(path: String)
    external fun stopRecording()
}