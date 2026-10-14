package com.example.intellicane.ai

import android.content.Context
import android.graphics.Bitmap
import android.util.Log

/**
 * AiVisionCoordinator
 * 
 * Orchestrates the end-to-end AI vision and voice pipeline:
 * 1. Triggers photo capture on cane via BLE command.
 * 2. Reassembles incoming BLE image chunks via BleImageReceiver.
 * 3. Sends assembled JPEG to Google Gemini Vision via GeminiVisionService.
 * 4. Speaks the resulting description aloud via CaneTtsManager.
 */
class AiVisionCoordinator(
    context: Context,
    private val sendBleCommand: (cmd: String) -> Unit,
    private val uiListener: UiListener? = null
) : BleImageReceiver.ImageReceiverListener {

    companion object {
        private const val TAG = "AiVisionCoordinator"
    }

    interface UiListener {
        fun onStatusUpdate(status: String)
        fun onImageReceived(bitmap: Bitmap)
        fun onDescriptionReady(description: String)
        fun onTransferStarted()
        fun onTransferError(reason: String)
    }

    val imageReceiver = BleImageReceiver(this)
    val geminiService = GeminiVisionService()
    val ttsManager = CaneTtsManager(context)

    private var isAnalyzing = false

    /**
     * Request the cane to capture a photo and stream it back over BLE.
     */
    fun requestCaptureAndDescribe() {
        if (isAnalyzing || imageReceiver.isTransferActive()) {
            ttsManager.speak("Photo capture already in progress.")
            return
        }

        uiListener?.onStatusUpdate("Requesting photo from cane...")
        ttsManager.speak("Capturing photo.", flush = true)

        // Command sent to ESP32-C3 over Nordic UART Service
        sendBleCommand("CMD:CAPTURE\n")
    }

    /**
     * Inspects every line received from the BLE characteristic.
     * @return true if handled by image protocol, false if normal telemetry
     */
    fun processIncomingBleLine(line: String): Boolean {
        return imageReceiver.handleIncomingLine(line)
    }

    // ========================================================================
    // BleImageReceiver.ImageReceiverListener Implementation
    // ========================================================================

    override fun onTransferStarted(expectedBytes: Int, totalChunks: Int) {
        val msg = "Receiving image from cane ($expectedBytes bytes, $totalChunks chunks)..."
        Log.i(TAG, msg)
        uiListener?.onStatusUpdate("Receiving photo: 0%")
        uiListener?.onTransferStarted()
    }

    override fun onTransferProgress(chunkIndex: Int, totalChunks: Int, percent: Int) {
        uiListener?.onStatusUpdate("Receiving photo: $percent%")
    }

    override fun onImageReceived(bitmap: Bitmap, rawJpeg: ByteArray) {
        uiListener?.onStatusUpdate("Photo received. Analyzing with Gemini AI...")
        uiListener?.onImageReceived(bitmap)

        ttsManager.speak("Analyzing.", flush = true)
        isAnalyzing = true

        geminiService.describeScene(
            rawJpeg,
            onSuccess = { description ->
                isAnalyzing = false
                Log.i(TAG, "Gemini description received: $description")
                uiListener?.onStatusUpdate("Analysis complete")
                uiListener?.onDescriptionReady(description)
                ttsManager.speak(description, flush = true)
            },
            onError = { error ->
                isAnalyzing = false
                Log.e(TAG, "Gemini analysis error: ${error.message}")
                val fallback = "Unable to analyze scene at this moment. Please check internet connection."
                uiListener?.onStatusUpdate("AI Analysis failed: ${error.message}")
                uiListener?.onDescriptionReady(fallback)
                ttsManager.speak(fallback, flush = true)
            }
        )
    }

    override fun onTransferError(reason: String) {
        isAnalyzing = false
        Log.e(TAG, "Image transfer error: $reason")
        uiListener?.onStatusUpdate("Transfer failed: $reason")
        uiListener?.onTransferError(reason)
        ttsManager.speak("Photo transfer failed. Please try again.", flush = true)
    }

    fun shutdown() {
        ttsManager.shutdown()
        imageReceiver.resetTransfer()
    }
}
