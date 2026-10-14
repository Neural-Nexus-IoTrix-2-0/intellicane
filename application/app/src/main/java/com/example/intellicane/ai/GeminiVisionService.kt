package com.example.intellicane.ai

import android.os.Handler
import android.os.Looper
import android.util.Base64
import android.util.Log
import org.json.JSONArray
import org.json.JSONObject
import java.io.BufferedReader
import java.io.InputStreamReader
import java.io.OutputStreamWriter
import java.net.HttpURLConnection
import java.net.URL
import java.util.concurrent.Executors

/**
 * GeminiVisionService
 * 
 * Communicates with the Google Gemini Multimodal Vision API to produce fast,
 * assistive spoken scene descriptions for blind and visually impaired users.
 */
class GeminiVisionService(
    private var apiKey: String = DEFAULT_API_KEY,
    private val model: String = "gemini-1.5-flash"
) {
    companion object {
        private const val TAG = "GeminiVisionService"
        // Set your Google Gemini API key or inject via BuildConfig / App Preferences
        const val DEFAULT_API_KEY = ""
        private const val SYSTEM_PROMPT =
            "You are the real-time AI assistive vision eyes for a blind person walking with an intelligent cane. " +
            "Analyze the image and provide a concise, natural, 1 to 2 sentence audio description of what is immediately " +
            "ahead of them. Explicitly mention immediate obstacles (chairs, stairs, curbs, people, poles) and clear walking pathways. " +
            "Speak directly to the user (e.g., 'A chair is one meter ahead on your left, path is clear to the right.'). " +
            "Keep it crisp, calm, and navigation-focused."
    }

    private val executor = Executors.newSingleThreadExecutor()
    private val mainHandler = Handler(Looper.getMainLooper())

    fun setApiKey(newKey: String) {
        if (newKey.isNotBlank()) {
            this.apiKey = newKey
        }
    }

    /**
     * Sends the JPEG image bytes to Gemini Vision and returns the generated scene description.
     */
    fun describeScene(
        jpegBytes: ByteArray,
        onSuccess: (description: String) -> Unit,
        onError: (error: Throwable) -> Unit
    ) {
        executor.execute {
            try {
                val description = callGeminiApi(jpegBytes)
                mainHandler.post { onSuccess(description) }
            } catch (e: Exception) {
                Log.e(TAG, "Gemini Vision API request failed: ${e.message}", e)
                mainHandler.post { onError(e) }
            }
        }
    }

    @Throws(Exception::class)
    private fun callGeminiApi(jpegBytes: ByteArray): String {
        if (apiKey.isBlank() || apiKey == "YOUR_GEMINI_API_KEY") {
            return "Photo captured. Please configure your Gemini API key in app settings to enable live scene description."
        }

        val base64Image = Base64.encodeToString(jpegBytes, Base64.NO_WRAP)
        val endpoint = "https://generativelanguage.googleapis.com/v1beta/models/$model:generateContent?key=$apiKey"

        val requestJson = JSONObject().apply {
            val contentsArray = JSONArray()
            val contentObj = JSONObject().apply {
                val partsArray = JSONArray()

                // Text Prompt part
                partsArray.put(JSONObject().apply {
                    put("text", SYSTEM_PROMPT)
                })

                // Image Data part
                partsArray.put(JSONObject().apply {
                    val inlineData = JSONObject().apply {
                        put("mime_type", "image/jpeg")
                        put("data", base64Image)
                    }
                    put("inline_data", inlineData)
                })

                put("parts", partsArray)
            }
            contentsArray.put(contentObj)
            put("contents", contentsArray)

            // Generation config: fast and concise
            val genConfig = JSONObject().apply {
                put("maxOutputTokens", 120)
                put("temperature", 0.4)
            }
            put("generationConfig", genConfig)
        }

        val url = URL(endpoint)
        val connection = url.openConnection() as HttpURLConnection
        try {
            connection.requestMethod = "POST"
            connection.setRequestProperty("Content-Type", "application/json; charset=UTF-8")
            connection.setRequestProperty("Accept", "application/json")
            connection.connectTimeout = 10000
            connection.readTimeout = 15000
            connection.doOutput = true
            connection.doInput = true

            // Send payload
            OutputStreamWriter(connection.outputStream, "UTF-8").use { writer ->
                writer.write(requestJson.toString())
                writer.flush()
            }

            val statusCode = connection.responseCode
            val responseStream = if (statusCode in 200..299) {
                connection.inputStream
            } else {
                connection.errorStream ?: connection.inputStream
            }

            val responseText = BufferedReader(InputStreamReader(responseStream, "UTF-8")).use { it.readText() }

            if (statusCode !in 200..299) {
                Log.e(TAG, "Gemini API error ($statusCode): $responseText")
                throw RuntimeException("Gemini Vision API error HTTP $statusCode: $responseText")
            }

            return parseGeminiResponse(responseText)
        } finally {
            connection.disconnect()
        }
    }

    private fun parseGeminiResponse(jsonString: String): String {
        val root = JSONObject(jsonString)
        val candidates = root.optJSONArray("candidates")
        if (candidates == null || candidates.length() == 0) {
            return "No obstacles or scene details detected."
        }

        val firstCandidate = candidates.getJSONObject(0)
        val content = firstCandidate.optJSONObject("content")
        val parts = content?.optJSONArray("parts")
        if (parts == null || parts.length() == 0) {
            return "Unable to describe current view."
        }

        val sb = StringBuilder()
        for (i in 0 until parts.length()) {
            val part = parts.getJSONObject(i)
            val text = part.optString("text")
            if (text.isNotBlank()) {
                sb.append(text.trim()).append(" ")
            }
        }

        val result = sb.toString().trim()
        return if (result.isNotEmpty()) result else "Clear path directly ahead."
    }
}
