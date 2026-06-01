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
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
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
import kotlin.math.cos
import kotlin.math.sin
import kotlin.random.Random

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
        wavFilePath = "${cacheDir.absolutePath}/magics_studio_record.wav"

        if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED) {
            permissionGranted = true
            VoiceEngine.startEngine()
        } else {
            requestPermissionLauncher.launch(Manifest.permission.RECORD_AUDIO)
        }

        setContent {
            MaterialTheme(colorScheme = darkColorScheme(background = StudioBackground, surface = SurfaceDark, primary = AccentCyan, secondary = AccentRed)) {
                if (!permissionGranted) {
                    Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                        Text("Microphone permission REQUIRED.", color = AccentRed)
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
    Scaffold(bottomBar = { StudioBottomNavigation(navController) }) { innerPadding ->
        NavHost(navController = navController, startDestination = "studio", modifier = Modifier.padding(innerPadding)) {
            composable("studio") { StudioScreen(wavFilePath) }
            composable("time_fx") { TimeFxScreen() }
            composable("mod_fx") { ModFxScreen() }
            composable("tone_fx") { ToneFxScreen() }
            composable("export") { ExportScreen(wavFilePath) }
        }
    }
}

@Composable
fun StudioBottomNavigation(navController: NavController) {
    val items = listOf("studio", "time_fx", "mod_fx", "tone_fx", "export")
    val icons = listOf(Icons.Default.Mic, Icons.Default.AccessTime, Icons.Default.Waves, Icons.Default.GraphicEq, Icons.Default.Share)
    val labels = listOf("Studio", "Time FX", "Mod FX", "Tone FX", "Export")

    val navBackStackEntry by navController.currentBackStackEntryAsState()
    val currentRoute = navBackStackEntry?.destination?.route

    NavigationBar(containerColor = SurfaceDark) {
        items.forEachIndexed { index, screen ->
            NavigationBarItem(
                icon = { Icon(icons[index], contentDescription = labels[index]) },
                label = { Text(labels[index], fontSize = 10.sp) },
                selected = currentRoute == screen,
                onClick = {
                    navController.navigate(screen) {
                        popUpTo(navController.graph.startDestinationId) { saveState = true }
                        launchSingleTop = true
                        restoreState = true
                    }
                },
                colors = NavigationBarItemDefaults.colors(selectedIconColor = StudioBackground, selectedTextColor = AccentCyan, indicatorColor = AccentCyan, unselectedIconColor = Color.Gray, unselectedTextColor = Color.Gray)
            )
        }
    }
}

@Composable
fun StudioScreen(wavFilePath: String) {
    var isRecording by remember { mutableStateOf(false) }

    Column(modifier = Modifier.fillMaxSize().background(StudioBackground).padding(24.dp), horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.Center) {
        Text("RECORDING CONSOLE", fontSize = 24.sp, fontWeight = FontWeight.Bold, color = Color.White, letterSpacing = 2.sp)
        Spacer(modifier = Modifier.height(40.dp))
        WaveformVisualizer(isRecording)
        Spacer(modifier = Modifier.height(60.dp))

        Button(
            onClick = {
                if (isRecording) { VoiceEngine.stopRecording(); isRecording = false }
                else { VoiceEngine.startRecording(wavFilePath); isRecording = true }
            },
            modifier = Modifier.size(120.dp), shape = RoundedCornerShape(60.dp),
            colors = ButtonDefaults.buttonColors(containerColor = if (isRecording) AccentRed else SurfaceDark, contentColor = Color.White),
            elevation = ButtonDefaults.buttonElevation(defaultElevation = 8.dp)
        ) {
            Text(if (isRecording) "STOP" else "REC", fontSize = 20.sp, fontWeight = FontWeight.Bold)
        }
    }
}

@Composable
fun WaveformVisualizer(isRecording: Boolean) {
    var amplitude by remember { mutableFloatStateOf(0f) }
    LaunchedEffect(isRecording) { while (isRecording) { amplitude = VoiceEngine.getAmplitude(); delay(50) }; amplitude = 0f }
    val animatedAmp by animateFloatAsState(targetValue = amplitude, label = "amp_anim")

    Card(modifier = Modifier.fillMaxWidth().height(150.dp), colors = CardDefaults.cardColors(containerColor = SurfaceDark), shape = RoundedCornerShape(12.dp)) {
        Canvas(modifier = Modifier.fillMaxSize().padding(16.dp)) {
            val barWidth = 12f; val spacing = 6f; val count = (size.width / (barWidth + spacing)).toInt(); val centerY = size.height / 2
            for (i in 0 until count) {
                val randomFactor = if (isRecording) Random.nextFloat() else 0.1f
                val barHeight = (size.height * animatedAmp * randomFactor * 2.5f).coerceIn(4f, size.height)
                drawLine(color = AccentCyan, start = Offset(i * (barWidth + spacing), centerY - barHeight / 2), end = Offset(i * (barWidth + spacing), centerY + barHeight / 2), strokeWidth = barWidth, cap = StrokeCap.Round)
            }
        }
    }
}

@Composable
fun TimeFxScreen() {
    Column(modifier = Modifier.fillMaxSize().background(StudioBackground).verticalScroll(rememberScrollState()).padding(16.dp)) {
        Text("TIME & SPACE", fontSize = 22.sp, fontWeight = FontWeight.Bold, color = AccentCyan)
        Spacer(modifier = Modifier.height(16.dp))

        // DELAY WITH TAP (Sa rotirajucim knobovima)
        var delayOn by remember { mutableStateOf(false) }
        var timeL by remember { mutableFloatStateOf(300f) }
        var timeR by remember { mutableFloatStateOf(300f) }
        var feedback by remember { mutableFloatStateOf(0.5f) }
        var volume by remember { mutableFloatStateOf(0.5f) }
        var lastTapTime by remember { mutableLongStateOf(0L) }

        fun upDelay() = VoiceEngine.setDelayParams(delayOn, timeL, timeR, feedback, volume)
        StudioModuleCard("Pro Stereo Delay", delayOn, onToggle = { delayOn = it; upDelay() }) {
            Button(onClick = {
                val now = System.currentTimeMillis()
                if (lastTapTime > 0 && (now - lastTapTime) < 2000) {
                    val tappedTime = (now - lastTapTime).toFloat().coerceIn(10f, 1000f)
                    timeL = tappedTime; timeR = tappedTime; upDelay()
                }
                lastTapTime = now
            }, modifier = Modifier.fillMaxWidth(), colors = ButtonDefaults.buttonColors(containerColor = AccentCyan)) { Text("TAP TEMPO", color = Color.Black) }

            Spacer(modifier = Modifier.height(16.dp))
            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceEvenly) {
                StudioKnob("Time L", timeL, 10f, 1000f) { timeL = it; upDelay() }
                StudioKnob("Time R", timeR, 10f, 1000f) { timeR = it; upDelay() }
                StudioKnob("F.Back", feedback, 0.0f, 0.95f) { feedback = it; upDelay() }
                StudioKnob("Mix", volume, 0.0f, 1.0f) { volume = it; upDelay() }
            }
        }

        // REVERB (Sa rotirajucim knobovima)
        var revOn by remember { mutableStateOf(false) }
        var rSize by remember { mutableFloatStateOf(0.7f) }
        var rDamp by remember { mutableFloatStateOf(0.5f) }
        var rMix by remember { mutableFloatStateOf(0.3f) }
        fun upRev() = VoiceEngine.setReverbParams(revOn, rSize, rDamp, rMix)
        StudioModuleCard("Solaris Reverb", revOn, onToggle = { revOn = it; upRev() }) {
            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceEvenly) {
                StudioKnob("Size", rSize, 0.1f, 1.0f) { rSize = it; upRev() }
                StudioKnob("Filter", rDamp, 0.0f, 1.0f) { rDamp = it; upRev() }
                StudioKnob("Mix", rMix, 0.0f, 1.0f) { rMix = it; upRev() }
            }
        }
        Spacer(modifier = Modifier.height(32.dp))
    }
}

@Composable
fun ModFxScreen() {
    Column(modifier = Modifier.fillMaxSize().background(StudioBackground).verticalScroll(rememberScrollState()).padding(16.dp)) {
        Text("MODULATION", fontSize = 22.sp, fontWeight = FontWeight.Bold, color = AccentCyan)
        Spacer(modifier = Modifier.height(16.dp))

        // CHORUS (Sa rotirajucim knobovima)
        var choOn by remember { mutableStateOf(false) }
        var choDms by remember { mutableFloatStateOf(7.5f) }
        var choDep by remember { mutableFloatStateOf(6.5f) }
        var choFrq by remember { mutableFloatStateOf(1.5f) }
        var choMix by remember { mutableFloatStateOf(0.5f) }
        fun upCho() = VoiceEngine.setChorusParams(choOn, choDms, choDep, choFrq, choMix)
        StudioModuleCard("Tracktion Chorus", choOn, onToggle = { choOn = it; upCho() }) {
            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceEvenly) {
                StudioKnob("Delay", choDms, 1.0f, 20.0f) { choDms = it; upCho() }
                StudioKnob("Depth", choDep, 0.1f, 10.0f) { choDep = it; upCho() }
                StudioKnob("Rate", choFrq, 0.1f, 5.0f) { choFrq = it; upCho() }
                StudioKnob("Mix", choMix, 0.0f, 1.0f) { choMix = it; upCho() }
            }
        }

        // FLANGER (Slajderi)
        var flaOn by remember { mutableStateOf(false) }
        var flaDms by remember { mutableFloatStateOf(5.0f) }
        var flaDep by remember { mutableFloatStateOf(100.0f) }
        var flaFrq by remember { mutableFloatStateOf(0.6f) }
        var flaFb by remember { mutableFloatStateOf(0.75f) }
        var flaMix by remember { mutableFloatStateOf(0.5f) }
        fun upFla() = VoiceEngine.setFlangerParams(flaOn, flaDms, flaDep, flaFrq, flaFb, flaMix)
        StudioModuleCard("Blue Cat Flanger", flaOn, onToggle = { flaOn = it; upFla() }) {
            StudioSliderRow("Delay (ms)", flaDms, 1.0f, 10.0f) { flaDms = it; upFla() }
            StudioSliderRow("Depth %", flaDep, 10.0f, 100.0f) { flaDep = it; upFla() }
            StudioSliderRow("Rate Hz", flaFrq, 0.1f, 5.0f) { flaFrq = it; upFla() }
            StudioSliderRow("Feedback", flaFb, 0.0f, 0.95f) { flaFb = it; upFla() }
            StudioSliderRow("Mix", flaMix, 0.0f, 1.0f) { flaMix = it; upFla() }
        }

        // PHASER (Slajderi)
        var phaOn by remember { mutableStateOf(false) }
        var phaRate by remember { mutableFloatStateOf(1.0f) }
        var phaDep by remember { mutableFloatStateOf(0.8f) }
        var phaFb by remember { mutableFloatStateOf(0.6f) }
        fun upPha() = VoiceEngine.setPhaserParams(phaOn, phaRate, phaDep, phaFb)
        StudioModuleCard("Vintage Phaser", phaOn, onToggle = { phaOn = it; upPha() }) {
            StudioSliderRow("Speed", phaRate, 0.1f, 5.0f) { phaRate = it; upPha() }
            StudioSliderRow("Depth", phaDep, 0.1f, 1.0f) { phaDep = it; upPha() }
            StudioSliderRow("Feedback", phaFb, 0.0f, 0.9f) { phaFb = it; upPha() }
        }

        // AUTO FILTER (Slajderi)
        var afOn by remember { mutableStateOf(false) }
        var afCut by remember { mutableFloatStateOf(896.0f) }
        var afRes by remember { mutableFloatStateOf(50.0f) }
        var afRate by remember { mutableFloatStateOf(1.0f) }
        var afDep by remember { mutableFloatStateOf(31.0f) }
        fun upAf() = VoiceEngine.setAutoFilterParams(afOn, afCut, afRes, afRate, afDep)
        StudioModuleCard("Auto Filter", afOn, onToggle = { afOn = it; upAf() }) {
            StudioSliderRow("Cutoff Hz", afCut, 20f, 5000f) { afCut = it; upAf() }
            StudioSliderRow("Resonance %", afRes, 0f, 100f) { afRes = it; upAf() }
            StudioSliderRow("LFO Rate", afRate, 0.1f, 10.0f) { afRate = it; upAf() }
            StudioSliderRow("Cutoff Mod %", afDep, 0f, 100f) { afDep = it; upAf() }
        }
        Spacer(modifier = Modifier.height(32.dp))
    }
}

@Composable
fun ToneFxScreen() {
    Column(modifier = Modifier.fillMaxSize().background(StudioBackground).verticalScroll(rememberScrollState()).padding(16.dp)) {
        Text("TONE & DYNAMICS", fontSize = 22.sp, fontWeight = FontWeight.Bold, color = AccentCyan)
        Spacer(modifier = Modifier.height(16.dp))

        // COMPRESSOR (Sa rotirajucim knobovima)
        var cmpOn by remember { mutableStateOf(false) }
        var cThr by remember { mutableFloatStateOf(-30f) }
        var cRat by remember { mutableFloatStateOf(2f) }
        var cAtt by remember { mutableFloatStateOf(50f) }
        var cRel by remember { mutableFloatStateOf(500f) }
        fun upCmp() = VoiceEngine.setCompParams(cmpOn, cThr, cRat, cAtt, cRel)
        StudioModuleCard("Tracktion Compressor", cmpOn, onToggle = { cmpOn = it; upCmp() }) {
            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceEvenly) {
                StudioKnob("Thresh", cThr, -60f, 0f) { cThr = it; upCmp() }
                StudioKnob("Ratio", cRat, 1f, 20f) { cRat = it; upCmp() }
                StudioKnob("Attack", cAtt, 1f, 200f) { cAtt = it; upCmp() }
                StudioKnob("Release", cRel, 10f, 1000f) { cRel = it; upCmp() }
            }
        }

        // AMPLIFIER (Sa rotirajucim knobovima)
        var ampOn by remember { mutableStateOf(false) }
        var aDrv by remember { mutableFloatStateOf(5f) }
        var aTon by remember { mutableFloatStateOf(5000f) }
        var aOut by remember { mutableFloatStateOf(1f) }
        fun upAmp() = VoiceEngine.setAmpParams(ampOn, aDrv, aTon, aOut)
        StudioModuleCard("Classic Amp", ampOn, onToggle = { ampOn = it; upAmp() }) {
            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceEvenly) {
                StudioKnob("Drive", aDrv, 1f, 20f) { aDrv = it; upAmp() }
                StudioKnob("Tone Hz", aTon, 500f, 10000f) { aTon = it; upAmp() }
                StudioKnob("Master", aOut, 0f, 2f) { aOut = it; upAmp() }
            }
        }

        // PITCH SHIFT (Slajderi)
        var pitOn by remember { mutableStateOf(false) }
        var semi by remember { mutableFloatStateOf(0f) }
        fun upPit() = VoiceEngine.setPitchParams(pitOn, semi)
        StudioModuleCard("Pitch Shifter", pitOn, onToggle = { pitOn = it; upPit() }) {
            StudioSliderRow("Semitones", semi, -12f, 12f) { semi = it; upPit() }
        }

        // OCTAVE (Slajderi)
        var octOn by remember { mutableStateOf(false) }
        var octMix by remember { mutableFloatStateOf(0.5f) }
        var octSemi by remember { mutableFloatStateOf(-12f) }
        fun upOct() = VoiceEngine.setOctaveParams(octOn, octMix, octSemi)
        StudioModuleCard("Octave Generator", octOn, onToggle = { octOn = it; upOct() }) {
            StudioSliderRow("Mix", octMix, 0f, 1f) { octMix = it; upOct() }
            StudioSliderRow("Shift", octSemi, -24f, 24f) { octSemi = it; upOct() }
        }

        // AUTO TUNE (Slajderi)
        var tunOn by remember { mutableStateOf(false) }
        var tCorr by remember { mutableFloatStateOf(1f) }
        var tSpd by remember { mutableFloatStateOf(1f) }
        fun upTun() = VoiceEngine.setTuneParams(tunOn, tCorr, tSpd)
        StudioModuleCard("Auto Tune", tunOn, onToggle = { tunOn = it; upTun() }) {
            StudioSliderRow("Correction", tCorr, 0.1f, 2.0f) { tCorr = it; upTun() }
            StudioSliderRow("Speed", tSpd, 0.1f, 2.0f) { tSpd = it; upTun() }
        }

        // VOCODER PROXY (Slajderi)
        var vocOn by remember { mutableStateOf(false) }
        var vocFreq by remember { mutableFloatStateOf(150f) }
        var vocMix by remember { mutableFloatStateOf(0.5f) }
        fun upVoc() = VoiceEngine.setVocoderParams(vocOn, vocFreq, vocMix)
        StudioModuleCard("Robot Vocoder", vocOn, onToggle = { vocOn = it; upVoc() }) {
            StudioSliderRow("Carrier Freq", vocFreq, 50f, 1000f) { vocFreq = it; upVoc() }
            StudioSliderRow("Mix", vocMix, 0f, 1f) { vocMix = it; upVoc() }
        }

        // 5-BAND EQ (Slajderi kao na pravim miksetama)
        var eqOn by remember { mutableStateOf(false) }
        var lg by remember { mutableFloatStateOf(0f) }
        var lmg by remember { mutableFloatStateOf(0f) }
        var mg by remember { mutableFloatStateOf(0f) }
        var hmg by remember { mutableFloatStateOf(0f) }
        var hg by remember { mutableFloatStateOf(0f) }
        fun upEq() {
            VoiceEngine.setEqEnabled(eqOn)
            VoiceEngine.setEqBand(0, lg, 80f, 0.707f); VoiceEngine.setEqBand(1, lmg, 300f, 1.0f)
            VoiceEngine.setEqBand(2, mg, 1000f, 1.0f); VoiceEngine.setEqBand(3, hmg, 4000f, 1.0f); VoiceEngine.setEqBand(4, hg, 10000f, 0.707f)
        }
        StudioModuleCard("5-Band EQ", eqOn, onToggle = { eqOn = it; upEq() }) {
            StudioSliderRow("Low", lg, -15f, 15f) { lg = it; upEq() }
            StudioSliderRow("Low Mid", lmg, -15f, 15f) { lmg = it; upEq() }
            StudioSliderRow("Mid", mg, -15f, 15f) { mg = it; upEq() }
            StudioSliderRow("High Mid", hmg, -15f, 15f) { hmg = it; upEq() }
            StudioSliderRow("High", hg, -15f, 15f) { hg = it; upEq() }
        }
        Spacer(modifier = Modifier.height(32.dp))
    }
}

// JUCE-STYLE ROTARY KNOB COMPONENT
@Composable
fun StudioKnob(
    label: String,
    value: Float,
    min: Float,
    max: Float,
    onValueChange: (Float) -> Unit
) {
    var angle by remember(value) {
        mutableFloatStateOf(-150f + ((value - min) / (max - min)) * 300f)
    }

    Column(
        horizontalAlignment = Alignment.CenterHorizontally,
        modifier = Modifier.padding(4.dp)
    ) {
        Canvas(
            modifier = Modifier
                .size(55.dp) // Optimizovana velicina da stanu 4 komada u red
                .pointerInput(Unit) {
                    detectDragGestures { change, dragAmount ->
                        change.consume()
                        val sensitivity = 1.5f
                        angle = (angle + dragAmount.x * sensitivity - dragAmount.y * sensitivity).coerceIn(-150f, 150f)
                        val newValue = min + ((angle + 150f) / 300f) * (max - min)
                        onValueChange(newValue)
                    }
                }
        ) {
            drawArc(
                color = Color.DarkGray,
                startAngle = 120f,
                sweepAngle = 300f,
                useCenter = false,
                style = Stroke(width = 6f, cap = StrokeCap.Round)
            )
            drawArc(
                color = AccentCyan,
                startAngle = 120f,
                sweepAngle = angle + 150f,
                useCenter = false,
                style = Stroke(width = 6f, cap = StrokeCap.Round)
            )
            drawCircle(color = SurfaceDark, radius = size.minDimension / 2 - 8f)

            val indicatorLength = size.minDimension / 2 - 12f
            val rad = (angle - 90f) * (Math.PI / 180f).toFloat()
            drawLine(
                color = Color.White,
                start = center,
                end = Offset(
                    x = center.x + indicatorLength * cos(rad).toFloat(),
                    y = center.y + indicatorLength * sin(rad).toFloat()
                ),
                strokeWidth = 3f,
                cap = StrokeCap.Round
            )
        }
        Spacer(modifier = Modifier.height(4.dp))
        Text(label, color = Color.LightGray, fontSize = 10.sp)
        Text(String.format("%.2f", value), color = Color.White, fontSize = 10.sp, fontWeight = FontWeight.Bold)
    }
}

@Composable
fun StudioModuleCard(title: String, enabled: Boolean, onToggle: (Boolean) -> Unit, content: @Composable () -> Unit) {
    Card(modifier = Modifier.fillMaxWidth().padding(vertical = 8.dp), colors = CardDefaults.cardColors(containerColor = SurfaceDark), shape = RoundedCornerShape(8.dp)) {
        Column(modifier = Modifier.padding(16.dp)) {
            Row(modifier = Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.SpaceBetween) {
                Text(title, fontSize = 18.sp, fontWeight = FontWeight.Bold, color = if(enabled) AccentCyan else Color.Gray)
                Switch(checked = enabled, onCheckedChange = onToggle, colors = SwitchDefaults.colors(checkedThumbColor = AccentCyan, checkedTrackColor = AccentCyan.copy(alpha=0.5f)))
            }
            if (enabled) { Spacer(modifier = Modifier.height(12.dp)); content() }
        }
    }
}

@Composable
fun StudioSliderRow(label: String, value: Float, min: Float, max: Float, onValueChange: (Float) -> Unit) {
    Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.fillMaxWidth().height(40.dp)) {
        Text(label, modifier = Modifier.weight(0.35f), fontSize = 13.sp, color = Color.LightGray)
        Slider(value = value, onValueChange = onValueChange, valueRange = min..max, modifier = Modifier.weight(0.5f), colors = SliderDefaults.colors(thumbColor = AccentCyan, activeTrackColor = AccentCyan))
        Text(text = String.format("%.2f", value), modifier = Modifier.weight(0.15f).padding(start = 8.dp), fontSize = 12.sp, color = Color.White)
    }
}

@Composable
fun ExportScreen(wavFilePath: String) {
    val context = androidx.compose.ui.platform.LocalContext.current
    Column(modifier = Modifier.fillMaxSize().background(StudioBackground).padding(24.dp), horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.Center) {
        Icon(Icons.Default.Share, contentDescription = "Export", tint = AccentCyan, modifier = Modifier.size(80.dp))
        Spacer(modifier = Modifier.height(24.dp))
        Text("MASTERING & EXPORT", fontSize = 22.sp, fontWeight = FontWeight.Bold, color = Color.White)
        Spacer(modifier = Modifier.height(48.dp))
        Button(onClick = { if (context is MainActivity) context.shareWavFile() }, modifier = Modifier.fillMaxWidth().height(60.dp), shape = RoundedCornerShape(8.dp), colors = ButtonDefaults.buttonColors(containerColor = AccentCyan)) {
            Text("SHARE BOUNCED WAV", fontSize = 18.sp, fontWeight = FontWeight.Bold, color = StudioBackground)
        }
    }
}