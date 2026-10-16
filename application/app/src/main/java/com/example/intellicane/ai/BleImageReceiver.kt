package com.example.intellicane.ai

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Matrix
import android.os.Handler
import android.os.Looper
import android.util.Base64
import android.util.Log
import java.io.ByteArrayOutputStream

/**
 * BleImageReceiver
 * 
 * Reassembles packetized image streams received from the intelligent cane over
 * Bluetooth Low Energy (BLE NUS).
 * 
 * Wire Protocol (text-safe lines over Nordic UART Service):
 *   IMG:START:<total_bytes>:<total_chunks>
 *   IMG:CHUNK:<chunk_index>:<base64_payload>
 *   IMG:END
 *   IMG:ERROR:<reason>
 */
class BleImageReceiver(
    private val listener: ImageReceiverListener
) {
    companion object {
        private const val TAG = "BleImageReceiver"
        private const val TIMEOUT_MS = 12000L
    }

    interface ImageReceiverListener {
        fun onTransferStarted(expectedBytes: Int, totalChunks: Int)
        fun onTransferProgress(chunkIndex: Int, totalChunks: Int, percent: Int)
        fun onImageReceived(bitmap: Bitmap, rawJpeg: ByteArray)
        fun onTransferError(reason: String)
    }

    private var isTransferring = false
    private var expectedBytes = 0
    private var totalChunks = 0
    private var receivedChunks = 0
    private val chunkMap = HashMap<Int, ByteArray>()
    private val handler = Handler(Looper.getMainLooper())

    private val timeoutRunnable = Runnable {
        if (isTransferring) {
            Log.e(TAG, "Image transfer timed out after $TIMEOUT_MS ms")
            resetTransfer()
            listener.onTransferError("Image transfer timed out. Partial chunks: $receivedChunks / $totalChunks")
        }
    }

    /**
     * Inspects an incoming BLE line.
     * @return true if the line was handled as an image protocol frame; false if normal telemetry.
     */
    fun handleIncomingLine(line: String): Boolean {
        val trimmed = line.trim()
        if (!trimmed.startsWith("IMG:")) {
            return false
        }

        resetTimeoutTimer()

        val parts = trimmed.split(":", limit = 4)
        val command = parts.getOrNull(1) ?: return true

        when (command) {
            "START" -> {
                // Format: IMG:START:<total_bytes>:<total_chunks>
                resetTransfer()
                isTransferring = true
                expectedBytes = parts.getOrNull(2)?.toIntOrNull() ?: 0
                totalChunks = parts.getOrNull(3)?.toIntOrNull() ?: 0
                receivedChunks = 0
                Log.d(TAG, "Starting image transfer: $expectedBytes bytes in $totalChunks chunks")
                listener.onTransferStarted(expectedBytes, totalChunks)
            }

            "CHUNK" -> {
                // Format: IMG:CHUNK:<chunk_index>:<base64_payload>
                if (!isTransferring) {
                    Log.w(TAG, "Received CHUNK while not transferring. Discarding.")
                    return true
                }
                val chunkIdx = parts.getOrNull(2)?.toIntOrNull() ?: return true
                val b64Payload = parts.getOrNull(3) ?: return true

                try {
                    val rawBytes = Base64.decode(b64Payload, Base64.DEFAULT)
                    chunkMap[chunkIdx] = rawBytes
                    receivedChunks++

                    val percent = if (totalChunks > 0) (receivedChunks * 100) / totalChunks else 0
                    listener.onTransferProgress(chunkIdx, totalChunks, percent)
                } catch (e: Exception) {
                    Log.e(TAG, "Failed to decode base64 chunk $chunkIdx: ${e.message}")
                }
            }

            "END" -> {
                if (!isTransferring) return true
                assembleAndEmitImage()
            }

            "ERROR" -> {
                val errorMsg = parts.getOrNull(2) ?: "Unknown cane camera error"
                Log.e(TAG, "Cane camera error reported: $errorMsg")
                resetTransfer()
                listener.onTransferError(errorMsg)
            }
        }

        return true
    }

    private fun assembleAndEmitImage() {
        cancelTimeoutTimer()
        isTransferring = false

        if (chunkMap.isEmpty()) {
            listener.onTransferError("No image chunks received")
            return
        }

        try {
            val outputStream = ByteArrayOutputStream()
            for (i in 0 until totalChunks) {
                val chunk = chunkMap[i]
                if (chunk != null) {
                    outputStream.write(chunk)
                } else {
                    Log.w(TAG, "Missing chunk $i / $totalChunks during assembly")
                }
            }
            val completeBytes = outputStream.toByteArray()
            Log.d(TAG, "Assembled complete image: ${completeBytes.size} bytes (expected: $expectedBytes)")

            val rawBitmap = BitmapFactory.decodeByteArray(completeBytes, 0, completeBytes.size)
            if (rawBitmap != null) {
                val rotatedBitmap = rotateBitmapAntiClockwise(rawBitmap)
                val rotatedStream = ByteArrayOutputStream()
                rotatedBitmap.compress(Bitmap.CompressFormat.JPEG, 90, rotatedStream)
                val rotatedJpegBytes = rotatedStream.toByteArray()

                listener.onImageReceived(rotatedBitmap, rotatedJpegBytes)
            } else {
                listener.onTransferError("Failed to decode image bytes into Bitmap (corrupt JPEG)")
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error assembling image: ${e.message}", e)
            listener.onTransferError("Assembly error: ${e.message}")
        } finally {
            chunkMap.clear()
        }
    }

    private fun rotateBitmapAntiClockwise(source: Bitmap): Bitmap {
        val matrix = Matrix()
        matrix.postRotate(-90f) // -90 degrees anti-clockwise
        return Bitmap.createBitmap(source, 0, 0, source.width, source.height, matrix, true)
    }

    fun isTransferActive(): Boolean = isTransferring

    fun resetTransfer() {
        cancelTimeoutTimer()
        isTransferring = false
        expectedBytes = 0
        totalChunks = 0
        receivedChunks = 0
        chunkMap.clear()
    }

    private fun resetTimeoutTimer() {
        handler.removeCallbacks(timeoutRunnable)
        handler.postDelayed(timeoutRunnable, TIMEOUT_MS)
    }

    private fun cancelTimeoutTimer() {
        handler.removeCallbacks(timeoutRunnable)
    }
}
