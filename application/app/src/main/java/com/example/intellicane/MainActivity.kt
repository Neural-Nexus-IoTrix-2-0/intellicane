package com.example.intellicane

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothProfile
import android.content.pm.PackageManager
import android.content.res.ColorStateList
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.drawable.BitmapDrawable
import android.graphics.drawable.Drawable
import android.location.Location
import android.location.LocationListener
import android.location.LocationManager
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.View
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ProgressBar
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
import com.google.android.material.textfield.TextInputEditText
import com.google.firebase.auth.FirebaseAuth
import com.google.firebase.database.DataSnapshot
import com.google.firebase.database.DatabaseError
import com.google.firebase.database.DatabaseReference
import com.google.firebase.database.FirebaseDatabase
import com.google.firebase.database.ServerValue
import com.google.firebase.database.ValueEventListener
import com.google.firebase.firestore.FieldValue
import com.google.firebase.firestore.FirebaseFirestore
import com.google.firebase.firestore.FirebaseFirestoreException
import com.google.firebase.firestore.ListenerRegistration
import com.google.firebase.firestore.SetOptions
import org.osmdroid.config.Configuration
import org.osmdroid.tileprovider.tilesource.OnlineTileSourceBase
import org.osmdroid.util.GeoPoint
import org.osmdroid.util.MapTileIndex
import org.osmdroid.views.MapView
import org.osmdroid.views.overlay.Marker
import java.text.SimpleDateFormat
import java.util.ArrayDeque
import java.util.Locale
import java.util.Queue
import java.util.UUID

class MainActivity : AppCompatActivity() {

    private lateinit var auth: FirebaseAuth
    private lateinit var db: FirebaseFirestore
    private var isSignUpMode = false

    private lateinit var layoutAuth: CardView
    private lateinit var tvAuthTitle: TextView
    private lateinit var etEmail: TextInputEditText
    private lateinit var etPassword: TextInputEditText
    private lateinit var btnSubmitAuth: Button
    private lateinit var tvToggleAuthMode: TextView

    private lateinit var layoutLoading: LinearLayout
    private lateinit var progressBar: ProgressBar
    private lateinit var tvLoadingMessage: TextView

    private lateinit var layoutTopBar: LinearLayout
    private lateinit var btnSignOut: Button
    private lateinit var btnBluetooth: Button
    private lateinit var btnToggleLocationSharing: Button

    private lateinit var cardCaretakerStatus: CardView
    private lateinit var tvUserPresenceStatus: TextView
    private lateinit var tvLocationSharingStatus: TextView
    private lateinit var tvLastLocationTime: TextView

    private lateinit var layoutSelection: LinearLayout
    private lateinit var btnUser: Button
    private lateinit var btnCaretaker: Button

    private lateinit var cardTerminal: CardView
    private lateinit var scrollTerminal: ScrollView
    private lateinit var tvTerminalOutput: TextView
    private lateinit var tvClearTerminal: TextView

    private lateinit var cardMap: CardView
    private lateinit var mapView: MapView
    private lateinit var fabMyLocation: FloatingActionButton
    private var userMarker: Marker? = null
    private var caretakerMarker: Marker? = null
    private var lastLocation: Location? = null

    // Bluetooth
    private var bluetoothGatt: BluetoothGatt? = null
    private var isConnected = false
    private val targetMacAddress = "48:F6:EE:17:C3:42"

    private val cccDescriptorUuid: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
    private val descriptorQueue: Queue<Pair<BluetoothGattCharacteristic, BluetoothGattDescriptor>> = ArrayDeque()

    // Presence & Location Sharing
    private var presenceRef: DatabaseReference? = null
    private var connectedRef: DatabaseReference? = null
    private var connectedValueListener: ValueEventListener? = null

    private var isLocationSharingActive = false
    private val locationSharingHandler = Handler(Looper.getMainLooper())
    private val locationSharingRunnable = object : Runnable {
        override fun run() {
            if (isLocationSharingActive) {
                sendCurrentLocationToFirestore()
                locationSharingHandler.postDelayed(this, 30000L) // every 30 seconds
            }
        }
    }

    // Caretaker Monitoring Listeners
    private var pairedPresenceRef: DatabaseReference? = null
    private var pairedPresenceListener: ValueEventListener? = null
    private var pairedLocationRegistration: ListenerRegistration? = null

    private val notificationPermissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestPermission()
    ) { _ -> }

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
            if (isLocationSharingActive) {
                sendCurrentLocationToFirestore()
            }
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
            if (isLocationSharingActive) {
                requestFreshLocationForUser()
            }
        } else {
            Toast.makeText(this, "Location permission is required for location tracking.", Toast.LENGTH_LONG).show()
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

        auth = FirebaseAuth.getInstance()
        db = FirebaseFirestore.getInstance()

        Configuration.getInstance().userAgentValue = "IntelliCaneApp/1.0 (Android; Navigation)"
        Configuration.getInstance().load(applicationContext, getSharedPreferences("osm_prefs", MODE_PRIVATE))

        enableEdgeToEdge()
        setContentView(R.layout.activity_main)
        ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main)) { v, insets ->
            val systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars())
            v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom)
            insets
        }

        // Auth UI
        layoutAuth = findViewById(R.id.layoutAuth)
        tvAuthTitle = findViewById(R.id.tvAuthTitle)
        etEmail = findViewById(R.id.etEmail)
        etPassword = findViewById(R.id.etPassword)
        btnSubmitAuth = findViewById(R.id.btnSubmitAuth)
        tvToggleAuthMode = findViewById(R.id.tvToggleAuthMode)

        // Loading Overlay
        layoutLoading = findViewById(R.id.layoutLoading)
        progressBar = findViewById(R.id.progressBar)
        tvLoadingMessage = findViewById(R.id.tvLoadingMessage)

        // Top Bar
        layoutTopBar = findViewById(R.id.layoutTopBar)
        btnSignOut = findViewById(R.id.btnSignOut)
        btnBluetooth = findViewById(R.id.btnBluetooth)
        btnToggleLocationSharing = findViewById(R.id.btnToggleLocationSharing)

        // Caretaker Status
        cardCaretakerStatus = findViewById(R.id.cardCaretakerStatus)
        tvUserPresenceStatus = findViewById(R.id.tvUserPresenceStatus)
        tvLocationSharingStatus = findViewById(R.id.tvLocationSharingStatus)
        tvLastLocationTime = findViewById(R.id.tvLastLocationTime)

        cardCaretakerStatus.setOnClickListener {
            promptForPairedUserUid()
        }

        // Selection & Mode Views
        layoutSelection = findViewById(R.id.layoutSelection)
        btnUser = findViewById(R.id.btnUser)
        btnCaretaker = findViewById(R.id.btnCaretaker)

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
                updateCaretakerMapLocation(it.latitude, it.longitude)
            } ?: run {
                startLocationUpdates()
            }
        }

        btnToggleLocationSharing.setOnClickListener {
            if (isLocationSharingActive) {
                stopLocationSharing()
            } else {
                startLocationSharing()
            }
        }

        tvToggleAuthMode.setOnClickListener {
            isSignUpMode = !isSignUpMode
            if (isSignUpMode) {
                tvAuthTitle.text = "Create a new account"
                btnSubmitAuth.text = "Create account"
                tvToggleAuthMode.text = "Already have an account? Sign In"
            } else {
                tvAuthTitle.text = "Sign In to your account"
                btnSubmitAuth.text = "Sign In"
                tvToggleAuthMode.text = "Don't have an account? Sign Up"
            }
        }

        btnSubmitAuth.setOnClickListener {
            handleAuthSubmit()
        }

        btnSignOut.setOnClickListener {
            signOutUser()
        }

        btnUser.setOnClickListener {
            saveUserRoleToFirestore("user")
        }

        btnCaretaker.setOnClickListener {
            saveUserRoleToFirestore("caretaker")
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

        checkAuthState()
    }

    private fun promptForPairedUserUid() {
        val input = TextInputEditText(this)
        input.hint = "User Firebase UID (e.g. abc123xyz)"

        AlertDialog.Builder(this)
            .setTitle("Pair User UID")
            .setMessage("Enter the Firebase Auth UID of the user account you want to monitor:")
            .setView(input)
            .setPositiveButton("Save & Monitor") { _, _ ->
                val userUid = input.text?.toString()?.trim() ?: ""
                if (userUid.isNotEmpty()) {
                    val caretakerUid = auth.currentUser?.uid ?: return@setPositiveButton
                    val updateData = mapOf("pairedUserUid" to userUid)
                    db.collection("users").document(caretakerUid).set(updateData, SetOptions.merge())
                        .addOnSuccessListener {
                            Toast.makeText(this, "Paired user UID saved!", Toast.LENGTH_SHORT).show()
                            startCaretakerMonitoring(userUid)
                        }
                        .addOnFailureListener { e ->
                            Toast.makeText(this, "Failed to save pairing: ${e.localizedMessage}", Toast.LENGTH_LONG).show()
                        }
                }
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun checkAuthState() {
        val currentUser = auth.currentUser
        if (currentUser == null) {
            stopUserPresence()
            stopCaretakerMonitoring()
            hideLoading()
            layoutAuth.visibility = View.VISIBLE
            layoutTopBar.visibility = View.GONE
            cardCaretakerStatus.visibility = View.GONE
            layoutSelection.visibility = View.GONE
            cardTerminal.visibility = View.GONE
            cardMap.visibility = View.GONE
        } else {
            layoutAuth.visibility = View.GONE
            layoutTopBar.visibility = View.VISIBLE
            setupUserPresence(currentUser.uid)
            requestNotificationPermission()
            loadUserRoleFromFirestore(currentUser.uid)
        }
    }

    private fun requestNotificationPermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
                notificationPermissionLauncher.launch(Manifest.permission.POST_NOTIFICATIONS)
            }
        }
    }

    private fun setupUserPresence(userUid: String) {
        val rtdb = FirebaseDatabase.getInstance()
        presenceRef = rtdb.getReference("status/users/$userUid")
        connectedRef = rtdb.getReference(".info/connected")

        connectedValueListener = object : ValueEventListener {
            override fun onDataChange(snapshot: DataSnapshot) {
                val connected = snapshot.getValue(Boolean::class.java) ?: false
                if (connected) {
                    presenceRef?.onDisconnect()?.setValue(
                        mapOf("state" to "offline", "lastChanged" to ServerValue.TIMESTAMP)
                    )
                    presenceRef?.setValue(
                        mapOf("state" to "online", "lastChanged" to ServerValue.TIMESTAMP)
                    )
                }
            }
            override fun onCancelled(error: DatabaseError) {}
        }
        connectedRef?.addValueEventListener(connectedValueListener as ValueEventListener)
    }

    private fun stopUserPresence() {
        connectedValueListener?.let { connectedRef?.removeEventListener(it) }
        presenceRef?.setValue(mapOf("state" to "offline", "lastChanged" to ServerValue.TIMESTAMP))
        connectedValueListener = null
        connectedRef = null
        presenceRef = null
    }

    private fun startLocationSharing() {
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

        isLocationSharingActive = true
        btnToggleLocationSharing.text = "Stop Sharing"
        btnToggleLocationSharing.backgroundTintList = ColorStateList.valueOf(Color.parseColor("#F44336"))

        startLocationUpdates()
        requestFreshLocationForUser()

        locationSharingHandler.removeCallbacks(locationSharingRunnable)
        locationSharingHandler.post(locationSharingRunnable)
        Toast.makeText(this, "Location sharing started (updates every 30s)", Toast.LENGTH_SHORT).show()
    }

    private fun stopLocationSharing() {
        isLocationSharingActive = false
        locationSharingHandler.removeCallbacks(locationSharingRunnable)
        btnToggleLocationSharing.text = "Share Loc"
        btnToggleLocationSharing.backgroundTintList = ColorStateList.valueOf(Color.parseColor("#2196F3"))

        val uid = auth.currentUser?.uid ?: return
        val stopMap = mapOf(
            "sharing" to false,
            "updatedAt" to FieldValue.serverTimestamp()
        )
        db.collection("users").document(uid).collection("location").document("current")
            .set(stopMap, SetOptions.merge())

        Toast.makeText(this, "Location sharing stopped", Toast.LENGTH_SHORT).show()
    }

    private fun requestFreshLocationForUser() {
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED &&
            ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_COARSE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
            return
        }

        val locationManager = getSystemService(LOCATION_SERVICE) as LocationManager
        if (locationManager.isProviderEnabled(LocationManager.GPS_PROVIDER)) {
            val lastGps = locationManager.getLastKnownLocation(LocationManager.GPS_PROVIDER)
            if (lastGps != null) {
                lastLocation = lastGps
                sendCurrentLocationToFirestore()
                return
            }
        }
        if (locationManager.isProviderEnabled(LocationManager.NETWORK_PROVIDER)) {
            val lastNet = locationManager.getLastKnownLocation(LocationManager.NETWORK_PROVIDER)
            if (lastNet != null) {
                lastLocation = lastNet
                sendCurrentLocationToFirestore()
            }
        }
    }

    private fun sendCurrentLocationToFirestore() {
        val uid = auth.currentUser?.uid ?: return
        val loc = lastLocation
        if (loc != null) {
            val locationData = mapOf(
                "sharing" to true,
                "latitude" to loc.latitude,
                "longitude" to loc.longitude,
                "updatedAt" to FieldValue.serverTimestamp()
            )
            db.collection("users").document(uid).collection("location").document("current")
                .set(locationData, SetOptions.merge())
                .addOnSuccessListener {
                    logToTerminal("[SYS] Location sent to Firestore: ${loc.latitude}, ${loc.longitude}")
                }
                .addOnFailureListener { e ->
                    logToTerminal("[SYS] Failed to send location: ${e.localizedMessage}")
                }
        } else {
            requestFreshLocationForUser()
        }
    }

    private fun loadUserRoleFromFirestore(uid: String) {
        showLoading("Loading account role...")
        layoutSelection.visibility = View.GONE
        cardTerminal.visibility = View.GONE
        cardMap.visibility = View.GONE
        cardCaretakerStatus.visibility = View.GONE

        db.collection("users").document(uid).get()
            .addOnSuccessListener { documentSnapshot ->
                hideLoading()
                if (documentSnapshot.exists()) {
                    val rawRole = documentSnapshot.getString("role")?.lowercase()
                    when (rawRole) {
                        "user" -> updateUIForRole("user", documentSnapshot.data)
                        "caretaker" -> updateUIForRole("caretaker", documentSnapshot.data)
                        else -> showRoleSelectionScreen()
                    }
                } else {
                    showRoleSelectionScreen()
                }
            }
            .addOnFailureListener { exception ->
                hideLoading()
                showRetryDialog(
                    title = "Connection Error",
                    message = "Failed to load user profile: ${exception.localizedMessage}",
                    onRetry = { loadUserRoleFromFirestore(uid) },
                    onCancel = { signOutUser() }
                )
            }
    }

    private fun saveUserRoleToFirestore(selectedRole: String) {
        val uid = auth.currentUser?.uid
        if (uid == null) {
            showErrorDialog("Error", "You must be signed in to select a role.")
            checkAuthState()
            return
        }

        showLoading("Saving selected role...")
        layoutSelection.visibility = View.GONE

        val updateData = mapOf("role" to selectedRole)
        db.collection("users").document(uid).set(updateData, SetOptions.merge())
            .addOnSuccessListener {
                hideLoading()
                Toast.makeText(this, "Role saved as $selectedRole", Toast.LENGTH_SHORT).show()
                loadUserRoleFromFirestore(uid)
            }
            .addOnFailureListener { exception ->
                hideLoading()
                showRetryDialog(
                    title = "Save Error",
                    message = "Failed to save user role: ${exception.localizedMessage}",
                    onRetry = { saveUserRoleToFirestore(selectedRole) },
                    onCancel = { showRoleSelectionScreen() }
                )
            }
    }

    private fun showRoleSelectionScreen() {
        hideLoading()
        layoutSelection.visibility = View.VISIBLE
        btnBluetooth.visibility = View.GONE
        btnToggleLocationSharing.visibility = View.GONE
        cardTerminal.visibility = View.GONE
        cardMap.visibility = View.GONE
        cardCaretakerStatus.visibility = View.GONE
    }

    private fun showLoading(message: String) {
        tvLoadingMessage.text = message
        layoutLoading.visibility = View.VISIBLE
    }

    private fun hideLoading() {
        layoutLoading.visibility = View.GONE
    }

    private fun showRetryDialog(title: String, message: String, onRetry: () -> Unit, onCancel: () -> Unit) {
        AlertDialog.Builder(this)
            .setTitle(title)
            .setMessage(message)
            .setCancelable(false)
            .setPositiveButton("Retry") { _, _ -> onRetry() }
            .setNegativeButton("Cancel") { _, _ -> onCancel() }
            .show()
    }

    private fun handleAuthSubmit() {
        val email = etEmail.text?.toString()?.trim() ?: ""
        val password = etPassword.text?.toString()?.trim() ?: ""

        if (email.isEmpty()) {
            etEmail.error = "Email address is required"
            return
        }

        if (password.isEmpty()) {
            etPassword.error = "Password is required"
            return
        }

        if (password.length < 6) {
            etPassword.error = "Password must be at least 6 characters"
            return
        }

        btnSubmitAuth.isEnabled = false

        if (isSignUpMode) {
            auth.createUserWithEmailAndPassword(email, password)
                .addOnCompleteListener(this) { task ->
                    btnSubmitAuth.isEnabled = true
                    if (task.isSuccessful) {
                        Toast.makeText(this, "Account created successfully!", Toast.LENGTH_SHORT).show()
                        etEmail.text?.clear()
                        etPassword.text?.clear()
                        checkAuthState()
                    } else {
                        val error = task.exception?.localizedMessage ?: "Registration failed."
                        showErrorDialog("Sign Up Error", error)
                    }
                }
        } else {
            auth.signInWithEmailAndPassword(email, password)
                .addOnCompleteListener(this) { task ->
                    btnSubmitAuth.isEnabled = true
                    if (task.isSuccessful) {
                        Toast.makeText(this, "Welcome back!", Toast.LENGTH_SHORT).show()
                        etEmail.text?.clear()
                        etPassword.text?.clear()
                        checkAuthState()
                    } else {
                        val error = task.exception?.localizedMessage ?: "Sign-in failed."
                        showErrorDialog("Sign In Error", error)
                    }
                }
        }
    }

    private fun signOutUser() {
        if (isConnected) {
            disconnectBluetoothDevice()
        }
        if (isLocationSharingActive) {
            stopLocationSharing()
        }
        stopCaretakerMonitoring()
        stopUserPresence()
        auth.signOut()
        Toast.makeText(this, "Signed out successfully", Toast.LENGTH_SHORT).show()
        checkAuthState()
    }

    private fun updateUIForRole(role: String, profileData: Map<String, Any>?) {
        hideLoading()
        layoutAuth.visibility = View.GONE
        layoutTopBar.visibility = View.VISIBLE
        layoutSelection.visibility = View.GONE

        if (role == "user") {
            stopCaretakerMonitoring()
            btnBluetooth.visibility = View.VISIBLE
            btnToggleLocationSharing.visibility = View.VISIBLE
            cardTerminal.visibility = View.VISIBLE
            cardMap.visibility = View.GONE
            cardCaretakerStatus.visibility = View.GONE
            updateBluetoothButtonState()
            startLocationUpdates()
        } else if (role == "caretaker") {
            btnBluetooth.visibility = View.GONE
            btnToggleLocationSharing.visibility = View.GONE
            cardTerminal.visibility = View.GONE
            cardMap.visibility = View.VISIBLE
            cardCaretakerStatus.visibility = View.VISIBLE

            findAndMonitorPairedUser(profileData)
        } else {
            showRoleSelectionScreen()
        }
    }

    private fun findAndMonitorPairedUser(profileData: Map<String, Any>?) {
        tvUserPresenceStatus.text = "Presence: Locating user..."
        tvLocationSharingStatus.text = "Sharing: Locating user..."
        tvLastLocationTime.text = "Checking database..."

        // 1. Check direct profile fields
        val explicitPairedUid = profileData?.get("pairedUserUid") as? String
            ?: profileData?.get("caretakerUid") as? String

        if (!explicitPairedUid.isNullOrEmpty()) {
            startCaretakerMonitoring(explicitPairedUid)
        } else {
            tvUserPresenceStatus.text = "Presence: Unpaired"
            tvLocationSharingStatus.text = "Sharing: Unpaired"
            tvLastLocationTime.text = "Tap here to enter paired User's UID."
        }
    }

    private fun startCaretakerMonitoring(pairedUserUid: String) {
        stopCaretakerMonitoring()

        tvUserPresenceStatus.text = "Presence: Connecting..."
        tvLocationSharingStatus.text = "Sharing: Connecting..."

        // 1. Listen to Realtime Database presence for paired user
        val rtdb = FirebaseDatabase.getInstance()
        pairedPresenceRef = rtdb.getReference("status/users/$pairedUserUid")
        pairedPresenceListener = object : ValueEventListener {
            override fun onDataChange(snapshot: DataSnapshot) {
                val state = snapshot.child("state").getValue(String::class.java)
                if (state == "online") {
                    tvUserPresenceStatus.text = "Presence: Online"
                    tvUserPresenceStatus.setTextColor(Color.parseColor("#4CAF50"))
                } else {
                    tvUserPresenceStatus.text = "Presence: Offline"
                    tvUserPresenceStatus.setTextColor(Color.parseColor("#757575"))
                }
            }
            override fun onCancelled(error: DatabaseError) {
                tvUserPresenceStatus.text = "Presence: Error (${error.message})"
            }
        }
        pairedPresenceRef?.addValueEventListener(pairedPresenceListener as ValueEventListener)

        // 2. Listen directly to Firestore users/{pairedUserUid}/location/current
        pairedLocationRegistration = db.collection("users")
            .document(pairedUserUid)
            .collection("location")
            .document("current")
            .addSnapshotListener { snapshot, error ->
                if (error != null) {
                    if (error.code == FirebaseFirestoreException.Code.PERMISSION_DENIED) {
                        tvLocationSharingStatus.text = "Sharing: Permission Denied"
                        tvLocationSharingStatus.setTextColor(Color.parseColor("#F44336"))
                        tvLastLocationTime.text = "Add 'caretakerUid': '${auth.currentUser?.uid}' to user's profile."
                    } else {
                        tvLocationSharingStatus.text = "Sharing: Error (${error.localizedMessage})"
                        tvLocationSharingStatus.setTextColor(Color.parseColor("#F44336"))
                        tvLastLocationTime.text = "Error reading location data."
                    }
                    return@addSnapshotListener
                }

                if (snapshot == null || !snapshot.exists()) {
                    tvLocationSharingStatus.text = "Sharing: Stopped (No data)"
                    tvLocationSharingStatus.setTextColor(Color.parseColor("#F44336"))
                    tvLastLocationTime.text = "No location document created yet."
                    return@addSnapshotListener
                }

                val isSharing = snapshot.getBoolean("sharing") ?: false
                val lat = snapshot.getDouble("latitude")
                val lon = snapshot.getDouble("longitude")
                val updatedAt = snapshot.getTimestamp("updatedAt")

                val now = System.currentTimeMillis()
                val updateTimeMillis = updatedAt?.toDate()?.time ?: 0L
                val isStale = (now - updateTimeMillis) > 90000L // 90 seconds threshold

                if (updatedAt != null) {
                    val sdf = SimpleDateFormat("hh:mm:ss a", Locale.getDefault())
                    tvLastLocationTime.text = "Last location update: ${sdf.format(updatedAt.toDate())}"
                } else {
                    tvLastLocationTime.text = "Last location update: Just now"
                }

                if (isSharing && !isStale && lat != null && lon != null) {
                    tvLocationSharingStatus.text = "Sharing: Active"
                    tvLocationSharingStatus.setTextColor(Color.parseColor("#4CAF50"))
                    updateUserMapLocation(lat, lon, "User's Location (Active)", Color.parseColor("#E53935")) // Vibrant Red Pin
                } else if (isSharing && isStale) {
                    tvLocationSharingStatus.text = "Sharing: Stale (>90s)"
                    tvLocationSharingStatus.setTextColor(Color.parseColor("#FF9800"))
                    if (lat != null && lon != null) {
                        updateUserMapLocation(lat, lon, "User's Location (Stale)", Color.parseColor("#FF9800")) // Orange Pin
                    }
                } else {
                    tvLocationSharingStatus.text = "Sharing: Stopped"
                    tvLocationSharingStatus.setTextColor(Color.parseColor("#F44336"))
                    if (lat != null && lon != null) {
                        updateUserMapLocation(lat, lon, "User's Location (Stopped)", Color.parseColor("#757575")) // Gray Pin
                    }
                }
            }
    }

    private fun updateUserMapLocation(lat: Double, lon: Double, titleText: String, pinColor: Int = Color.parseColor("#E53935")) {
        val geoPoint = GeoPoint(lat, lon)
        mapView.controller.setZoom(18.0)
        mapView.controller.animateTo(geoPoint)

        userMarker?.let { mapView.overlays.remove(it) }
        val marker = Marker(mapView)
        marker.position = geoPoint
        marker.setAnchor(Marker.ANCHOR_CENTER, Marker.ANCHOR_CENTER)
        marker.title = titleText
        marker.snippet = "Lat: %.5f, Lon: %.5f".format(lat, lon)
        marker.icon = createColoredMarkerIcon(pinColor)
        mapView.overlays.add(marker)
        userMarker = marker
        mapView.invalidate()
    }

    private fun updateCaretakerMapLocation(lat: Double, lon: Double) {
        val geoPoint = GeoPoint(lat, lon)
        mapView.controller.setZoom(17.5)
        mapView.controller.animateTo(geoPoint)

        caretakerMarker?.let { mapView.overlays.remove(it) }
        val marker = Marker(mapView)
        marker.position = geoPoint
        marker.setAnchor(Marker.ANCHOR_CENTER, Marker.ANCHOR_CENTER)
        marker.title = "Caretaker's Location (You)"
        marker.snippet = "Lat: %.5f, Lon: %.5f".format(lat, lon)
        marker.icon = createColoredMarkerIcon(Color.parseColor("#2196F3")) // Royal Blue Pin
        mapView.overlays.add(marker)
        caretakerMarker = marker
        mapView.invalidate()
    }

    private fun createColoredMarkerIcon(colorInt: Int): Drawable {
        val density = resources.displayMetrics.density
        val size = (38 * density).toInt()
        val bitmap = Bitmap.createBitmap(size, size, Bitmap.Config.ARGB_8888)
        val canvas = Canvas(bitmap)

        val paint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = colorInt
            style = Paint.Style.FILL
        }

        val borderPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = Color.WHITE
            style = Paint.Style.STROKE
            strokeWidth = 3f * density
        }

        val innerDotPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = Color.WHITE
            style = Paint.Style.FILL
        }

        val centerX = size / 2f
        val centerY = size / 2f
        val radius = (size / 2f) - (2f * density)

        // Draw outer colored pin circle
        canvas.drawCircle(centerX, centerY, radius, paint)
        // Draw white border
        canvas.drawCircle(centerX, centerY, radius, borderPaint)
        // Draw inner white center dot
        canvas.drawCircle(centerX, centerY, radius * 0.35f, innerDotPaint)

        return BitmapDrawable(resources, bitmap)
    }

    private fun stopCaretakerMonitoring() {
        pairedPresenceListener?.let { pairedPresenceRef?.removeEventListener(it) }
        pairedLocationRegistration?.remove()
        pairedPresenceListener = null
        pairedPresenceRef = null
        pairedLocationRegistration = null
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
            }
        }

        if (locationManager.isProviderEnabled(LocationManager.NETWORK_PROVIDER)) {
            locationManager.requestLocationUpdates(LocationManager.NETWORK_PROVIDER, 2000L, 5f, locationListener)
            val lastNet = locationManager.getLastKnownLocation(LocationManager.NETWORK_PROVIDER)
            if (lastNet != null && lastLocation == null) {
                lastLocation = lastNet
            }
        }
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
        stopUserPresence()
        stopCaretakerMonitoring()
        disconnectBluetoothDevice()
    }
}
