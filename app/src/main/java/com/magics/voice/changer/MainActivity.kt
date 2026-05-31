package com.magics.voice.changer

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Build
import androidx.compose.material.icons.filled.Mic
import androidx.compose.material.icons.filled.Share
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.ContextCompat
import androidx.core.content.FileProvider
import androidx.navigation.NavController
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.currentBackStackEntryAsState
import androidx.navigation.compose.rememberNavController
import kotlinx.coroutines.delay
import java.io.File
import kotlin.random.Random

// Global Color Scheme for High-End Studio Look
val StudioBackground = Color(0xFF121212)
val SurfaceDark = Color(0xFF1E1E1E)
val AccentCyan = Color(0xFF00E5FF)
val AccentRed = Color(0xFFFF1744)

class MainActivity : ComponentActivity() {

    private var permissionGranted by mutableStateOf(false)
    private lateinit var wavFilePath: String

    private val requestPermissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestPermission()
    ) { isGranted: Boolean ->
        permissionGranted = isGranted
        if (isGranted) VoiceEngine.startEngine()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Define standard cache path for WAV export
        wavFilePath = "${cacheDir.absolutePath}/magics_studio_record.wav"

        if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED) {
            permissionGranted = true
            VoiceEngine.startEngine()
        } else {
            requestPermissionLauncher.launch(Manifest.permission.RECORD_AUDIO)
        }

        setContent {
            MaterialTheme(
                colorScheme = darkColorScheme(
                    background = StudioBackground,
                    surface = SurfaceDark,
                    primary = AccentCyan,
                    secondary = AccentRed
                )
            ) {
                if (!permissionGranted) {
                    Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                        Text("Microphone permission required for Studio Mode.", color = AccentRed)
                    }
                } else {
                    MainAppNavHost(wavFilePath)
                }
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        VoiceEngine.stopEngine()
    }

    // Function to handle the actual sharing/export process
    fun shareWavFile() {
        val file = File(wavFilePath)
        if (!file.exists()) return

        val uri = FileProvider.getUriForFile(this, "${packageName}.fileprovider", file)
        val shareIntent = Intent(Intent.ACTION_SEND).apply {
            type = "audio/wav"
            putExtra(Intent.EXTRA_STREAM, uri)
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }
        startActivity(Intent.createChooser(shareIntent, "Share Mastered Audio"))
    }
}

@Composable
fun MainAppNavHost(wavFilePath: String) {
    val navController = rememberNavController()

    Scaffold(
        bottomBar = { StudioBottomNavigation(navController) }
    ) { innerPadding ->
        NavHost(
            navController = navController,
            startDestination = "studio",
            modifier = Modifier.padding(innerPadding)
        ) {
            composable("studio") { StudioScreen(wavFilePath) }
            composable("fx_rack") { FxRackScreen() }
            composable("export") { ExportScreen(wavFilePath) }
        }
    }
}

@Composable
fun StudioBottomNavigation(navController: NavController) {
    val items = listOf("studio", "fx_rack", "export")
    val icons = listOf(Icons.Default.Mic, Icons.Default.Build, Icons.Default.Share)
    val labels = listOf("Studio", "FX Rack", "Export")

    val navBackStackEntry by navController.currentBackStackEntryAsState()
    val currentRoute = navBackStackEntry?.destination?.route

    NavigationBar(containerColor = SurfaceDark) {
        items.forEachIndexed { index, screen ->
            NavigationBarItem(
                icon = { Icon(icons[index], contentDescription = labels[index]) },
                label = { Text(labels[index]) },
                selected = currentRoute == screen,
                onClick = {
                    navController.navigate(screen) {
                        popUpTo(navController.graph.startDestinationId) { saveState = true }
                        launchSingleTop = true
                        restoreState = true
                    }
                },
                colors = NavigationBarItemDefaults.colors(
                    selectedIconColor = StudioBackground,
                    selectedTextColor = AccentCyan,
                    indicatorColor = AccentCyan,
                    unselectedIconColor = Color.Gray,
                    unselectedTextColor = Color.Gray
                )
            )
        }
    }
}

@Composable
fun StudioScreen(wavFilePath: String) {
    var isRecording by remember { mutableStateOf(false) }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(StudioBackground)
            .padding(24.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.Center
    ) {
        Text("RECORDING CONSOLE", fontSize = 24.sp, fontWeight = FontWeight.Bold, color = Color.White, letterSpacing = 2.sp)
        Spacer(modifier = Modifier.height(40.dp))

        WaveformVisualizer(isRecording)

        Spacer(modifier = Modifier.height(60.dp))

        Button(
            onClick = {
                if (isRecording) {
                    VoiceEngine.stopRecording()
                    isRecording = false
                } else {
                    VoiceEngine.startRecording(wavFilePath)
                    isRecording = true
                }
            },
            modifier = Modifier
                .size(120.dp),
            shape = RoundedCornerShape(60.dp),
            colors = ButtonDefaults.buttonColors(
                containerColor = if (isRecording) AccentRed else SurfaceDark,
                contentColor = Color.White
            ),
            elevation = ButtonDefaults.buttonElevation(defaultElevation = 8.dp)
        ) {
            Text(if (isRecording) "STOP" else "REC", fontSize = 20.sp, fontWeight = FontWeight.Bold)
        }
    }
}

@Composable
fun WaveformVisualizer(isRecording: Boolean) {
    var amplitude by remember { mutableFloatStateOf(0f) }

    LaunchedEffect(isRecording) {
        while (isRecording) {
            amplitude = VoiceEngine.getAmplitude()
            delay(50) // Smooth 20fps refresh rate
        }
        amplitude = 0f
    }

    val animatedAmp by animateFloatAsState(targetValue = amplitude, label = "amp_anim")

    Card(
        modifier = Modifier
            .fillMaxWidth()
            .height(150.dp),
        colors = CardDefaults.cardColors(containerColor = SurfaceDark),
        shape = RoundedCornerShape(12.dp)
    ) {
        Canvas(modifier = Modifier.fillMaxSize().padding(16.dp)) {
            val barWidth = 12f
            val spacing = 6f
            val count = (size.width / (barWidth + spacing)).toInt()
            val centerY = size.height / 2

            for (i in 0 until count) {
                val randomFactor = if (isRecording) Random.nextFloat() else 0.1f
                val barHeight = (size.height * animatedAmp * randomFactor * 2.5f).coerceIn(4f, size.height)

                drawLine(
                    color = AccentCyan,
                    start = Offset(i * (barWidth + spacing), centerY - barHeight / 2),
                    end = Offset(i * (barWidth + spacing), centerY + barHeight / 2),
                    strokeWidth = barWidth,
                    cap = StrokeCap.Round
                )
            }
        }
    }
}

@Composable
fun FxRackScreen() {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(StudioBackground)
            .verticalScroll(rememberScrollState())
            .padding(16.dp)
    ) {
        Text("PRO DSP EFFECTS", fontSize = 22.sp, fontWeight = FontWeight.Bold, color = AccentCyan, letterSpacing = 1.sp)
        Spacer(modifier = Modifier.height(16.dp))

        // SoundTouch Pitch
        var pitchOn by remember { mutableStateOf(false) }
        var semitones by remember { mutableFloatStateOf(0f) }
        fun upPitch() = VoiceEngine.setPitchParams(pitchOn, semitones)
        StudioModuleCard("SoundTouch Pitch", pitchOn, onToggle = { pitchOn = it; upPitch() }) {
            StudioSliderRow("Semitones", semitones, -12f, 12f) { semitones = it; upPitch() }
        }

        // STK Chorus
        var chorusOn by remember { mutableStateOf(false) }
        var depth by remember { mutableFloatStateOf(0.2f) }
        var freq by remember { mutableFloatStateOf(1.5f) }
        fun upChorus() = VoiceEngine.setChorusParams(chorusOn, depth, freq)
        StudioModuleCard("STK Chorus", chorusOn, onToggle = { chorusOn = it; upChorus() }) {
            StudioSliderRow("Mod Depth", depth, 0.0f, 1.0f) { depth = it; upChorus() }
            StudioSliderRow("LFO Freq (Hz)", freq, 0.1f, 5.0f) { freq = it; upChorus() }
        }

        // STK Delay
        var delayOn by remember { mutableStateOf(false) }
        var time by remember { mutableFloatStateOf(300f) }
        var feedback by remember { mutableFloatStateOf(0.5f) }
        fun upDelay() = VoiceEngine.setDelayParams(delayOn, time, feedback)
        StudioModuleCard("STK Delay", delayOn, onToggle = { delayOn = it; upDelay() }) {
            StudioSliderRow("Time (ms)", time, 10f, 900f) { time = it; upDelay() }
            StudioSliderRow("Feedback", feedback, 0.0f, 0.9f) { feedback = it; upDelay() }
        }

        Spacer(modifier = Modifier.height(32.dp))
    }
}

@Composable
fun StudioModuleCard(title: String, enabled: Boolean, onToggle: (Boolean) -> Unit, content: @Composable () -> Unit) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 8.dp),
        colors = CardDefaults.cardColors(containerColor = SurfaceDark),
        shape = RoundedCornerShape(8.dp)
    ) {
        Column(modifier = Modifier.padding(16.dp)) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                Text(title, fontSize = 18.sp, fontWeight = FontWeight.Bold, color = if(enabled) AccentCyan else Color.Gray)
                Switch(
                    checked = enabled,
                    onCheckedChange = onToggle,
                    colors = SwitchDefaults.colors(checkedThumbColor = AccentCyan, checkedTrackColor = AccentCyan.copy(alpha=0.5f))
                )
            }
            if (enabled) {
                Spacer(modifier = Modifier.height(12.dp))
                content()
            }
        }
    }
}

@Composable
fun StudioSliderRow(label: String, value: Float, min: Float, max: Float, onValueChange: (Float) -> Unit) {
    Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.fillMaxWidth().height(40.dp)) {
        Text(label, modifier = Modifier.weight(0.35f), fontSize = 13.sp, color = Color.LightGray)
        Slider(
            value = value,
            onValueChange = onValueChange,
            valueRange = min..max,
            modifier = Modifier.weight(0.5f),
            colors = SliderDefaults.colors(thumbColor = AccentCyan, activeTrackColor = AccentCyan)
        )
        Text(
            text = String.format("%.2f", value),
            modifier = Modifier.weight(0.15f).padding(start = 8.dp),
            fontSize = 12.sp,
            color = Color.White
        )
    }
}

@Composable
fun ExportScreen(wavFilePath: String) {
    val context = androidx.compose.ui.platform.LocalContext.current

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(StudioBackground)
            .padding(24.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.Center
    ) {
        Icon(Icons.Default.Share, contentDescription = "Export", tint = AccentCyan, modifier = Modifier.size(80.dp))
        Spacer(modifier = Modifier.height(24.dp))
        Text("MASTERING & EXPORT", fontSize = 22.sp, fontWeight = FontWeight.Bold, color = Color.White)
        Spacer(modifier = Modifier.height(16.dp))
        Text("Your processed audio is ready to be shared or saved to external storage.", color = Color.Gray, fontSize = 14.sp)

        Spacer(modifier = Modifier.height(48.dp))

        Button(
            onClick = {
                if (context is MainActivity) {
                    context.shareWavFile()
                }
            },
            modifier = Modifier.fillMaxWidth().height(60.dp),
            shape = RoundedCornerShape(8.dp),
            colors = ButtonDefaults.buttonColors(containerColor = AccentCyan)
        ) {
            Text("SHARE BOUNCED WAV", fontSize = 18.sp, fontWeight = FontWeight.Bold, color = StudioBackground)
        }
    }
}