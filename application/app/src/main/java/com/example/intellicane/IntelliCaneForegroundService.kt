package com.example.intellicane

import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.os.Build
import android.os.IBinder
import androidx.core.app.NotificationCompat

object ServiceListener {
    var onDisconnectBle: (() -> Unit)? = null
    var onStopLocationSharing: (() -> Unit)? = null
}

class IntelliCaneForegroundService : Service() {

    companion object {
        const val CHANNEL_ID = "intellicane_foreground_service_channel"
        const val NOTIFICATION_ID = 2002

        const val ACTION_START_SERVICE = "com.example.intellicane.ACTION_START_SERVICE"
        const val ACTION_STOP_SERVICE = "com.example.intellicane.ACTION_STOP_SERVICE"
        const val ACTION_UPDATE_STATE = "com.example.intellicane.ACTION_UPDATE_STATE"

        const val ACTION_DISCONNECT_BLE = "com.example.intellicane.ACTION_DISCONNECT_BLE"
        const val ACTION_STOP_LOCATION_SHARING = "com.example.intellicane.ACTION_STOP_LOCATION_SHARING"

        const val EXTRA_BLE_CONNECTED = "extra_ble_connected"
        const val EXTRA_LOCATION_SHARING = "extra_location_sharing"

        fun startService(context: Context, bleConnected: Boolean, locationSharing: Boolean) {
            val intent = Intent(context, IntelliCaneForegroundService::class.java).apply {
                action = ACTION_START_SERVICE
                putExtra(EXTRA_BLE_CONNECTED, bleConnected)
                putExtra(EXTRA_LOCATION_SHARING, locationSharing)
            }
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                context.startForegroundService(intent)
            } else {
                context.startService(intent)
            }
        }

        fun updateState(context: Context, bleConnected: Boolean, locationSharing: Boolean) {
            val intent = Intent(context, IntelliCaneForegroundService::class.java).apply {
                action = ACTION_UPDATE_STATE
                putExtra(EXTRA_BLE_CONNECTED, bleConnected)
                putExtra(EXTRA_LOCATION_SHARING, locationSharing)
            }
            context.startService(intent)
        }

        fun stopService(context: Context) {
            val intent = Intent(context, IntelliCaneForegroundService::class.java).apply {
                action = ACTION_STOP_SERVICE
            }
            context.startService(intent)
        }
    }

    private var isBleConnected = false
    private var isLocationSharing = false

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onCreate() {
        super.onCreate()
        createNotificationChannel()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        val action = intent?.action

        when (action) {
            ACTION_START_SERVICE, ACTION_UPDATE_STATE -> {
                isBleConnected = intent.getBooleanExtra(EXTRA_BLE_CONNECTED, isBleConnected)
                isLocationSharing = intent.getBooleanExtra(EXTRA_LOCATION_SHARING, isLocationSharing)
                showOrUpdateNotification()
            }
            ACTION_DISCONNECT_BLE -> {
                isBleConnected = false
                ServiceListener.onDisconnectBle?.invoke()
                showOrUpdateNotification()
            }
            ACTION_STOP_LOCATION_SHARING -> {
                isLocationSharing = false
                ServiceListener.onStopLocationSharing?.invoke()
                showOrUpdateNotification()
            }
            ACTION_STOP_SERVICE -> {
                stopForeground(STOP_FOREGROUND_REMOVE)
                stopSelf()
            }
        }

        return START_STICKY
    }

    private fun showOrUpdateNotification() {
        val notificationIntent = Intent(this, MainActivity::class.java).apply {
            flags = Intent.FLAG_ACTIVITY_CLEAR_TOP or Intent.FLAG_ACTIVITY_SINGLE_TOP
        }
        val contentPendingIntent = PendingIntent.getActivity(
            this, 0, notificationIntent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )

        val statusText = when {
            isBleConnected && isLocationSharing -> "BLE Connected • Location Sharing Active"
            isBleConnected -> "BLE Connected • Location Sharing Stopped"
            isLocationSharing -> "BLE Disconnected • Location Sharing Active"
            else -> "Background Monitoring Active"
        }

        val builder = NotificationCompat.Builder(this, CHANNEL_ID)
            .setSmallIcon(R.mipmap.ic_launcher)
            .setContentTitle("IntelliCane Service Active")
            .setContentText(statusText)
            .setContentIntent(contentPendingIntent)
            .setOngoing(true)
            .setPriority(NotificationCompat.PRIORITY_LOW)

        // Action Button 1: Disconnect BLE
        if (isBleConnected) {
            val disconnectIntent = Intent(this, IntelliCaneForegroundService::class.java).apply {
                action = ACTION_DISCONNECT_BLE
            }
            val disconnectPendingIntent = PendingIntent.getService(
                this, 101, disconnectIntent,
                PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
            )
            builder.addAction(R.mipmap.ic_launcher, "Disconnect BLE", disconnectPendingIntent)
        }

        // Action Button 2: Stop Location Sharing
        if (isLocationSharing) {
            val stopSharingIntent = Intent(this, IntelliCaneForegroundService::class.java).apply {
                action = ACTION_STOP_LOCATION_SHARING
            }
            val stopSharingPendingIntent = PendingIntent.getService(
                this, 102, stopSharingIntent,
                PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
            )
            builder.addAction(R.mipmap.ic_launcher, "Stop Sharing", stopSharingPendingIntent)
        }

        val notification = builder.build()

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            val serviceType = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE or ServiceInfo.FOREGROUND_SERVICE_TYPE_LOCATION
            } else 0
            startForeground(NOTIFICATION_ID, notification, serviceType)
        } else {
            startForeground(NOTIFICATION_ID, notification)
        }
    }

    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                CHANNEL_ID,
                "IntelliCane Background Service",
                NotificationManager.IMPORTANCE_LOW
            ).apply {
                description = "Ongoing notification for IntelliCane background tasks and controls"
            }
            val manager = getSystemService(NOTIFICATION_SERVICE) as NotificationManager
            manager.createNotificationChannel(channel)
        }
    }
}
