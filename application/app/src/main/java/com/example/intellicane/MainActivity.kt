package com.example.intellicane

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothProfile
import android.content.Context
import android.content.SharedPreferences
import android.content.pm.PackageManager
import android.content.res.ColorStateList
import android.graphics.Color
import android.location.Location
import android.location.LocationListener
import android.location.LocationManager
import android.os.Build
import android.os.Bundle
import android.view.View
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.cardview.widget.CardView
import androidx.core.content.ContextCompat
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import com.google.android.material.floatingactionbutton.FloatingActionButton
import org.osmdroid.config.Configuration
import org.osmdroid.tileprovider.tilesource.OnlineTileSourceBase
import org.osmdroid.util.GeoPoint
import org.osmdroid.util.MapTileIndex
import org.osmdroid.views.MapView
import org.osmdroid.views.overlay.Marker
import java.util.ArrayDeque
import java.util.Queue
import java.util.UUID

class MainActivity : AppCompatActivity() {

    private lateinit var prefs: SharedPreferences
    private lateinit var layoutSelection: LinearLayout
    private lateinit var btnUser: Button
    private lateinit var btnCaretaker: Button
    private lateinit var btnBluetooth: Button

    private lateinit var cardTerminal: CardView
    private lateinit var scrollTerminal: ScrollView
    private lateinit var tvTerminalOutput: TextView
    private lateinit var tvClearTerminal: TextView

    private lateinit var cardMap: CardView
    private lateinit var mapView: MapView
    private lateinit var fabMyLocation: FloatingActionButton
    private var currentMarker: Marker? = null
    private var lastLocation: Location? = null

    private var bluetoothGatt: BluetoothGatt? = null
    private var isConnected = false
    private val targetMacAddress = "48:F6:EE:17:C3:42"

    private val cccDescriptorUuid: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
    private val descriptorQueue: Queue<Pair<BluetoothGattCharacteristic, BluetoothGattDescriptor>> = ArrayDeque()

    private val esriTileSource = object : OnlineTileSourceBase(
        "Esri_WorldStreetMap",
        0, 19, 256, "",
        arrayOf("https://server.arcgisonline.com/ArcGIS/rest/services/World_Street_Map/MapServer/tile/")
    ) {
        override fun getTileURLString(pMapTileIndex: Long): String {
            return baseUrl + MapTileIndex.getZoom(pMapTileIndex) + "/" +
                    MapTileIndex.getY(pMapTileIndex) + "/" +
                    MapTileIndex.getX(pMapTileIndex)
        }
    }

    private val locationListener = object : LocationListener {
        override fun onLocationChanged(location: Location) {
            lastLocation = location
            updateMapLocation(location.latitude, location.longitude)
        }

        @Deprecated("Deprecated in Java")
        override fun onStatusChanged(provider: String?, status: Int, extras: Bundle?) {}
        override fun onProviderEnabled(provider: String) {}
        override fun onProviderDisabled(provider: String) {}
    }

    private val locationPermissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { permissions ->
        val granted = permissions.entries.any { it.value }
        if (granted) {
            startLocationUpdates()
        } else {
            Toast.makeText(this, "Location permission is required to display your current location on the map.", Toast.LENGTH_LONG).show()
        }
    }

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt?, status: Int, newState: Int) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                isConnected = true
                bluetoothGatt = gatt
                logToTerminal("[SYS] BLE connected to $targetMacAddress")

                if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S ||
                    ContextCompat.checkSelfPermission(this@MainActivity, Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED) {
                    logToTerminal("[SYS] Requesting MTU 512...")
                    if (gatt?.requestMtu(512) == false) {
                        logToTerminal("[SYS] MTU request failed, discovering services...")
                        gatt.discoverServices()
                    }
                }

                runOnUiThread {
                    btnBluetooth.isEnabled = true
                    updateBluetoothButtonState()
                    Toast.makeText(this@MainActivity, "Connected to $targetMacAddress via BLE", Toast.LENGTH_SHORT).show()
                }
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                val wasConnected = isConnected
                isConnected = false
                descriptorQueue.clear()
                bluetoothGatt?.close()
                bluetoothGatt = null
                logToTerminal("[SYS] BLE Disconnected.")

                runOnUiThread {
                    btnBluetooth.isEnabled = true
                    updateBluetoothButtonState()
                    if (wasConnected) {
                        Toast.makeText(this@MainActivity, "Disconnected from $targetMacAddress", Toast.LENGTH_SHORT).show()
                    } else {
                        showErrorDialog(
                            "Device Unavailable",
                            "Unable to connect to BLE device ($targetMacAddress).\n\nPlease ensure the BLE device is powered on, advertising, and within range."
                        )
                    }
                }
            }
        }

        override fun onMtuChanged(gatt: BluetoothGatt?, mtu: Int, status: Int) {
            logToTerminal("[SYS] MTU set to $mtu. Discovering GATT services...")
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S ||
                ContextCompat.checkSelfPermission(this@MainActivity, Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED) {
                gatt?.discoverServices()
            }
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt?, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS && gatt != null) {
                logToTerminal("[SYS] Services discovered (${gatt.services.size} services). Preparing serial listener...")
                prepareBleNotifications(gatt)
            } else {
                logToTerminal("[SYS] Service discovery failed with status $status")
            }
        }

        override fun onDescriptorWrite(gatt: BluetoothGatt?, descriptor: BluetoothGattDescriptor?, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                logToTerminal("[SYS] Notification enabled for ${descriptor?.characteristic?.uuid}")
            } else {
                logToTerminal("[SYS] Descriptor write failed for ${descriptor?.characteristic?.uuid} status=$status")
            }
            if (gatt != null) {
                processNextDescriptorWrite(gatt)
            }
        }

        @Suppress("DEPRECATION")
        override fun onCharacteristicChanged(
            gatt: BluetoothGatt?,
            characteristic: BluetoothGattCharacteristic?
        ) {
            val bytes = characteristic?.value ?: return
            val incomingText = String(bytes, Charsets.UTF_8)
            logToTerminal(incomingText, isIncoming = true)
        }

        override fun onCharacteristicChanged(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            value: ByteArray
        ) {
            val incomingText = String(value, Charsets.UTF_8)
            logToTerminal(incomingText, isIncoming = true)
        }
    }

    private fun prepareBleNotifications(gatt: BluetoothGatt) {
        descriptorQueue.clear()
        for (service in gatt.services) {
            for (characteristic in service.characteristics) {
                val props = characteristic.properties
                val isNotifiable = (props and BluetoothGattCharacteristic.PROPERTY_NOTIFY) != 0
                val isIndicatable = (props and BluetoothGattCharacteristic.PROPERTY_INDICATE) != 0

                if (isNotifiable || isIndicatable) {
                    val descriptor = characteristic.getDescriptor(cccDescriptorUuid)
                    if (descriptor != null) {
                        @Suppress("DEPRECATION")
                        descriptor.value = if (isNotifiable) {
                            BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                        } else {
                            BluetoothGattDescriptor.ENABLE_INDICATION_VALUE
                        }
                        descriptorQueue.add(Pair(characteristic, descriptor))
                    }
                }
            }
        }

        logToTerminal("[SYS] Queued ${descriptorQueue.size} notification subscription(s). Processing...")
        processNextDescriptorWrite(gatt)
    }

    private fun processNextDescriptorWrite(gatt: BluetoothGatt) {
        if (descriptorQueue.isEmpty()) {
            logToTerminal("[SYS] BLE Serial ready. Listening for data...")
            return
        }
        val (characteristic, descriptor) = descriptorQueue.poll() ?: return
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S ||
            ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED) {
            gatt.setCharacteristicNotification(characteristic, true)
            @Suppress("DEPRECATION")
            val success = gatt.writeDescriptor(descriptor)
            if (!success) {
                logToTerminal("[SYS] Failed to write descriptor for ${characteristic.uuid}. Trying next...")
                processNextDescriptorWrite(gatt)
            }
        }
    }

    private fun logToTerminal(message: String, isIncoming: Boolean = false) {
        runOnUiThread {
            val formatted = if (isIncoming) message else "$message\n"
            tvTerminalOutput.append(formatted)
            scrollTerminal.post {
                scrollTerminal.fullScroll(View.FOCUS_DOWN)
            }
        }
    }

    private val permissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { permissions ->
        val allGranted = permissions.entries.all { it.value }
        if (allGranted) {
            connectToBleDevice()
        } else {
            showErrorDialog("Permission Required", "Bluetooth permissions are required to connect to the BLE device.")
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Set User-Agent for OSMDroid
        Configuration.getInstance().userAgentValue = "IntelliCaneApp/1.0 (Android; Navigation)"
        Configuration.getInstance().load(applicationContext, getSharedPreferences("osm_prefs", MODE_PRIVATE))

        enableEdgeToEdge()
        setContentView(R.layout.activity_main)
        ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main)) { v, insets ->
            val systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars())
            v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom)
            insets
        }

        prefs = getSharedPreferences("app_prefs", MODE_PRIVATE)
        layoutSelection = findViewById(R.id.layoutSelection)
        btnUser = findViewById(R.id.btnUser)
        btnCaretaker = findViewById(R.id.btnCaretaker)
        btnBluetooth = findViewById(R.id.btnBluetooth)

        cardTerminal = findViewById(R.id.cardTerminal)
        scrollTerminal = findViewById(R.id.scrollTerminal)
        tvTerminalOutput = findViewById(R.id.tvTerminalOutput)
        tvClearTerminal = findViewById(R.id.tvClearTerminal)

        cardMap = findViewById(R.id.cardMap)
        mapView = findViewById(R.id.mapView)
        fabMyLocation = findViewById(R.id.fabMyLocation)

        mapView.setTileSource(esriTileSource)
        mapView.setMultiTouchControls(true)

        tvClearTerminal.setOnClickListener {
            tvTerminalOutput.text = "> Terminal cleared.\n"
        }

        fabMyLocation.setOnClickListener {
            lastLocation?.let {
                updateMapLocation(it.latitude, it.longitude)
            } ?: run {
                startLocationUpdates()
            }
        }

        btnUser.setOnClickListener {
            saveAndSetMode("User")
        }

        btnCaretaker.setOnClickListener {
            saveAndSetMode("Caretaker")
        }

        btnBluetooth.setOnClickListener {
            if (isConnected) {
                disconnectBluetoothDevice()
            } else {
                if (hasBluetoothPermissions()) {
                    connectToBleDevice()
                } else {
                    requestBluetoothPermissions()
                }
            }
        }

        checkAndShowModeSelection()
    }

    private fun checkAndShowModeSelection() {
        val savedMode = prefs.getString("user_mode", null)
        if (savedMode == null) {
            layoutSelection.visibility = View.VISIBLE
            btnBluetooth.visibility = View.GONE
            cardTerminal.visibility = View.GONE
            cardMap.visibility = View.GONE
        } else {
            layoutSelection.visibility = View.GONE
            updateUIForMode(savedMode)
        }
    }

    private fun saveAndSetMode(mode: String) {
        prefs.edit().putString("user_mode", mode).apply()
        layoutSelection.visibility = View.GONE
        updateUIForMode(mode)
    }

    private fun updateUIForMode(mode: String) {
        if (mode == "User") {
            btnBluetooth.visibility = View.VISIBLE
            cardTerminal.visibility = View.VISIBLE
            cardMap.visibility = View.GONE
            updateBluetoothButtonState()
        } else {
            btnBluetooth.visibility = View.GONE
            cardTerminal.visibility = View.GONE
            cardMap.visibility = View.VISIBLE
            startLocationUpdates()
        }
    }

    private fun startLocationUpdates() {
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED &&
            ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_COARSE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
            locationPermissionLauncher.launch(
                arrayOf(
                    Manifest.permission.ACCESS_FINE_LOCATION,
                    Manifest.permission.ACCESS_COARSE_LOCATION
                )
            )
            return
        }

        val locationManager = getSystemService(LOCATION_SERVICE) as LocationManager

        if (locationManager.isProviderEnabled(LocationManager.GPS_PROVIDER)) {
            locationManager.requestLocationUpdates(LocationManager.GPS_PROVIDER, 2000L, 5f, locationListener)
            val lastGps = locationManager.getLastKnownLocation(LocationManager.GPS_PROVIDER)
            if (lastGps != null) {
                lastLocation = lastGps
                updateMapLocation(lastGps.latitude, lastGps.longitude)
            }
        }

        if (locationManager.isProviderEnabled(LocationManager.NETWORK_PROVIDER)) {
            locationManager.requestLocationUpdates(LocationManager.NETWORK_PROVIDER, 2000L, 5f, locationListener)
            val lastNet = locationManager.getLastKnownLocation(LocationManager.NETWORK_PROVIDER)
            if (lastNet != null && lastLocation == null) {
                lastLocation = lastNet
                updateMapLocation(lastNet.latitude, lastNet.longitude)
            }
        }
    }

    private fun updateMapLocation(lat: Double, lon: Double) {
        val geoPoint = GeoPoint(lat, lon)
        mapView.controller.setZoom(17.5)
        mapView.controller.animateTo(geoPoint)

        currentMarker?.let { mapView.overlays.remove(it) }
        val marker = Marker(mapView)
        marker.position = geoPoint
        marker.setAnchor(Marker.ANCHOR_CENTER, Marker.ANCHOR_BOTTOM)
        marker.title = "Current Phone Location"
        marker.snippet = "Lat: %.5f, Lon: %.5f".format(lat, lon)
        mapView.overlays.add(marker)
        currentMarker = marker
        mapView.invalidate()
    }

    private fun hasBluetoothPermissions(): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED
        } else {
            ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH) == PackageManager.PERMISSION_GRANTED
        }
    }

    private fun requestBluetoothPermissions() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            permissionLauncher.launch(
                arrayOf(
                    Manifest.permission.BLUETOOTH_CONNECT,
                    Manifest.permission.BLUETOOTH_SCAN
                )
            )
        } else {
            permissionLauncher.launch(
                arrayOf(
                    Manifest.permission.BLUETOOTH,
                    Manifest.permission.BLUETOOTH_ADMIN,
                    Manifest.permission.ACCESS_FINE_LOCATION
                )
            )
        }
    }

    private fun connectToBleDevice() {
        val bluetoothAdapter = BluetoothAdapter.getDefaultAdapter()
        if (bluetoothAdapter == null) {
            showErrorDialog("Bluetooth Unsupported", "Bluetooth is not supported on this device.")
            return
        }

        if (!bluetoothAdapter.isEnabled) {
            showErrorDialog("Bluetooth Disabled", "Please turn on Bluetooth on your device and try again.")
            return
        }

        btnBluetooth.isEnabled = false
        btnBluetooth.text = "Connecting..."
        logToTerminal("[SYS] Initiating BLE connection to $targetMacAddress...")

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S &&
            ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
            btnBluetooth.isEnabled = true
            updateBluetoothButtonState()
            showErrorDialog("Permission Denied", "Bluetooth connect permission missing.")
            return
        }

        try {
            val device: BluetoothDevice = bluetoothAdapter.getRemoteDevice(targetMacAddress)
            bluetoothGatt?.close()

            bluetoothGatt = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                device.connectGatt(this, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
            } else {
                device.connectGatt(this, false, gattCallback)
            }
        } catch (e: Exception) {
            btnBluetooth.isEnabled = true
            updateBluetoothButtonState()
            logToTerminal("[SYS] BLE connect error: ${e.localizedMessage}")
            showErrorDialog("Connection Error", "Error connecting to BLE device: ${e.localizedMessage}")
        }
    }

    private fun showErrorDialog(title: String, message: String) {
        AlertDialog.Builder(this)
            .setTitle(title)
            .setMessage(message)
            .setPositiveButton("OK", null)
            .show()
    }

    private fun disconnectBluetoothDevice() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S ||
            ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED) {
            bluetoothGatt?.disconnect()
            bluetoothGatt?.close()
        }
        bluetoothGatt = null
        isConnected = false
        descriptorQueue.clear()
        updateBluetoothButtonState()
        logToTerminal("[SYS] Disconnected by user.")
        Toast.makeText(this, "Disconnected from $targetMacAddress", Toast.LENGTH_SHORT).show()
    }

    private fun updateBluetoothButtonState() {
        if (isConnected) {
            btnBluetooth.text = "Disconnect"
            btnBluetooth.backgroundTintList = ColorStateList.valueOf(Color.parseColor("#F44336")) // Red
        } else {
            btnBluetooth.text = "Connect"
            btnBluetooth.backgroundTintList = ColorStateList.valueOf(Color.parseColor("#4CAF50")) // Green
        }
    }

    override fun onResume() {
        super.onResume()
        mapView.onResume()
    }

    override fun onPause() {
        super.onPause()
        mapView.onPause()
    }

    override fun onDestroy() {
        super.onDestroy()
        disconnectBluetoothDevice()
    }
}
