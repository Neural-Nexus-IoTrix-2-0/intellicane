package com.example.intellicane.ai

import android.content.Context
import android.speech.tts.TextToSpeech
import android.speech.tts.UtteranceProgressListener
import android.util.Log
import java.util.Locale
import java.util.UUID

/**
 * CaneTtsManager
 * 
 * Provides crisp, high-volume offline Text-To-Speech audio feedback for the user
 * through the phone's speaker or connected Bluetooth headphones.
 */
class CaneTtsManager(
    context: Context,
    private val onInitialized: ((ready: Boolean) -> Unit)? = null
) : TextToSpeech.OnInitListener {

    companion object {
        private const val TAG = "CaneTtsManager"
    }

    private var tts: TextToSpeech? = TextToSpeech(context.applicationContext, this)
    private var isReady = false
    private val pendingSpeechQueue = mutableListOf<String>()

    override fun onInit(status: Int) {
        if (status == TextToSpeech.SUCCESS) {
            val result = tts?.setLanguage(Locale.US)
            if (result == TextToSpeech.LANG_MISSING_DATA || result == TextToSpeech.LANG_NOT_SUPPORTED) {
                Log.w(TAG, "Locale US not supported for TTS, attempting default device locale...")
                tts?.language = Locale.getDefault()
            }
            tts?.setSpeechRate(1.05f) // Slightly faster for prompt assistive guidance
            tts?.setPitch(1.0f)

            tts?.setOnUtteranceProgressListener(object : UtteranceProgressListener() {
                override fun onStart(utteranceId: String?) {
                    Log.d(TAG, "TTS speech started: $utteranceId")
                }

                override fun onDone(utteranceId: String?) {
                    Log.d(TAG, "TTS speech completed: $utteranceId")
                }

                @Deprecated("Deprecated in Java")
                override fun onError(utteranceId: String?) {
                    Log.e(TAG, "TTS speech error on utterance: $utteranceId")
                }
            })

            isReady = true
            Log.i(TAG, "Text-to-Speech engine initialized successfully")
            onInitialized?.invoke(true)

            // Speak any utterances queued before init finished
            synchronized(pendingSpeechQueue) {
                for (pending in pendingSpeechQueue) {
                    speak(pending)
                }
                pendingSpeechQueue.clear()
            }
        } else {
            Log.e(TAG, "Failed to initialize Text-to-Speech engine (status: $status)")
            isReady = false
            onInitialized?.invoke(false)
        }
    }

    /**
     * Speaks the provided text aloud.
     * @param text The text message to speak.
     * @param flush If true, interrupts any current speech immediately. If false, queues it.
     */
    fun speak(text: String, flush: Boolean = true) {
        if (text.isBlank()) return

        if (!isReady || tts == null) {
            synchronized(pendingSpeechQueue) {
                if (flush) pendingSpeechQueue.clear()
                pendingSpeechQueue.add(text)
            }
            return
        }

        val queueMode = if (flush) TextToSpeech.QUEUE_FLUSH else TextToSpeech.QUEUE_ADD
        val utteranceId = UUID.randomUUID().toString()
        tts?.speak(text, queueMode, null, utteranceId)
    }

    fun stop() {
        tts?.stop()
    }

    fun shutdown() {
        try {
            tts?.stop()
            tts?.shutdown()
            tts = null
            isReady = false
        } catch (e: Exception) {
            Log.e(TAG, "Error shutting down TTS: ${e.message}")
        }
    }
}
