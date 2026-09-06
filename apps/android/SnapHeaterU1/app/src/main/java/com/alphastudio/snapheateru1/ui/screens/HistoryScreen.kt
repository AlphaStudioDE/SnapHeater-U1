package com.alphastudio.snapheateru1.ui.screens

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.*
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.FileDownload
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import com.alphastudio.snapheateru1.R
import com.alphastudio.snapheateru1.data.TemperatureHistory
import com.alphastudio.snapheateru1.data.TemperaturePoint
import com.alphastudio.snapheateru1.data.HistoryGap
import com.alphastudio.snapheateru1.model.HeaterSnapshot
import com.alphastudio.snapheateru1.ui.components.ActionLabel
import com.alphastudio.snapheateru1.ui.components.ScreenColumn
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import kotlinx.coroutines.launch
import kotlinx.coroutines.delay
import org.json.JSONObject
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

@Composable
fun HistoryScreen(history: TemperatureHistory, snapshot: HeaterSnapshot, recordingError: Boolean=false, telemetryFresh: Boolean=false, catchingUp: Boolean=false) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    var points by remember(snapshot.deviceId) { mutableStateOf(emptyList<TemperaturePoint>()) }
    var gaps by remember(snapshot.deviceId) { mutableStateOf(emptyList<HistoryGap>()) }
    var hours by remember { mutableStateOf(2) }
    var status by remember { mutableStateOf("") }
    var exporting by remember { mutableStateOf(false) }
    var eventLimit by remember(snapshot.deviceId) { mutableStateOf(100) }
    var events by remember(snapshot.deviceId) { mutableStateOf(emptyList<com.alphastudio.snapheateru1.data.RecordedEvent>()) }
    // Capture identity before the document picker; never export a newly selected heater.
    var exportDevice by remember { mutableStateOf("") }
    var report by remember { mutableStateOf("") }
    val done = stringResource(R.string.history_export_done)
    val failed = stringResource(R.string.history_error)
    val csv = rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument("text/csv")) { uri ->
        if (uri != null) scope.launch {
            exporting = true
            status = runCatching { withContext(Dispatchers.IO) {
                context.contentResolver.openOutputStream(uri, "wt")?.bufferedWriter()?.use { history.export(exportDevice, it) }
                    ?: error("Cannot open document")
            }; done }.getOrElse { failed }
            exporting = false
        }
    }
    val json = rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument("application/json")) { uri ->
        if (uri != null) scope.launch {
            exporting = true
            status = runCatching { withContext(Dispatchers.IO) {
                context.contentResolver.openOutputStream(uri, "wt")?.bufferedWriter()?.use { it.write(report) }
                    ?: error("Cannot open document")
            }; done }.getOrElse { failed }
            exporting = false
        }
    }
    LaunchedEffect(snapshot.deviceId, hours) {
        while (true) {
            runCatching { withContext(Dispatchers.IO) {
                val since=System.currentTimeMillis()-hours*3600000L
                history.read(snapshot.deviceId,since) to history.gaps(snapshot.deviceId,since)
            } }.onSuccess { points=it.first;gaps=it.second }.onFailure { status = failed }
            delay(10000)
        }
    }
    LaunchedEffect(snapshot.deviceId,eventLimit) {
        while(true) {
            runCatching {withContext(Dispatchers.IO) {
                com.alphastudio.snapheateru1.data.EventHistory(context).use {it.read(snapshot.deviceId,eventLimit)}
            }}.onSuccess {events=it}.onFailure {status=failed}
            delay(5000)
        }
    }
    ScreenColumn {
        Text(stringResource(R.string.history_title), style=MaterialTheme.typography.headlineMedium)
        if(recordingError) Text(stringResource(R.string.history_error))
        if(catchingUp) {LinearProgressIndicator(Modifier.fillMaxWidth());Text(stringResource(R.string.history_catching_up))}
        Text(stringResource(R.string.history_note))
        Row(horizontalArrangement=Arrangement.spacedBy(12.dp)) {
            FilterChip(selected=hours==2, onClick={ hours=2 }, label={Text("2 h")})
            FilterChip(selected=hours==24, onClick={ hours=24 }, label={Text("24 h")})
        }
        if (points.size<2) Text(stringResource(R.string.history_empty))
        else TemperatureChart(points,gaps)
        Text(stringResource(R.string.error_history_title),style=MaterialTheme.typography.titleLarge)
        Text(stringResource(R.string.error_history_note),style=MaterialTheme.typography.bodySmall)
        if(snapshot.sensorFreezeWarningMs>0) Text(
            if(telemetryFresh) stringResource(R.string.freeze_message,"SH_${snapshot.deviceId.takeLast(4)}",snapshot.sensorFreezeRemainingS)
            else stringResource(R.string.freeze_disconnected), color=MaterialTheme.colorScheme.error)
        if(events.isEmpty()) Text(stringResource(R.string.error_history_empty))
        val eventFormat=SimpleDateFormat("dd.MM.yyyy HH:mm:ss",Locale.getDefault())
        events.forEach { event ->
            OutlinedCard(Modifier.fillMaxWidth()) {
                Column(Modifier.padding(12.dp)) {
                    Text((if(event.approximate) "≈ " else "")+eventFormat.format(Date(event.time)),style=MaterialTheme.typography.labelMedium)
                    Text(com.alphastudio.snapheateru1.data.eventDescription(context,event.code),
                        color=if(event.level in listOf("error","critical")) MaterialTheme.colorScheme.error else MaterialTheme.colorScheme.onSurface)
                }
            }
        }
        if(events.size>=eventLimit && eventLimit<10000) TextButton(onClick={eventLimit+=100}) {Text(stringResource(R.string.error_history_more))}
        if(gaps.isNotEmpty()) {
            Text(stringResource(R.string.history_gaps),color=MaterialTheme.colorScheme.error)
            val format=SimpleDateFormat("dd.MM HH:mm",Locale.getDefault())
            gaps.takeLast(8).forEach { gap ->
                val reason=when(gap.kind) {
                    "restart" -> stringResource(R.string.history_restart)
                    "overwritten" -> stringResource(R.string.history_overwritten,gap.missing)
                    else -> stringResource(R.string.history_invalid_sensor)
                }
                Text(format.format(Date(gap.time))+" · "+reason,style=MaterialTheme.typography.bodySmall)
            }
            if(gaps.size>8) Text(stringResource(R.string.history_more_gaps,gaps.size-8))
        }
        Button(enabled=!exporting && snapshot.deviceId.isNotBlank(), onClick={
            exportDevice=snapshot.deviceId
            csv.launch("SnapHeater-history.csv")
        }) { ActionLabel(Icons.Outlined.FileDownload, stringResource(R.string.history_export)) }
        OutlinedButton(enabled=!exporting, onClick={
            // Explicit whitelist. No raw status JSON, Wi-Fi, tokens or network addresses.
            report=JSONObject().put("schema",1).put("captured_at_ms",System.currentTimeMillis())
                .put("firmware",snapshot.firmwareVersion).put("mode",snapshot.mode.name)
                .put("telemetry_fresh",telemetryFresh).put("readings_valid",snapshot.historyReadingValid)
                .put("paused",snapshot.paused).put("chamber_c",snapshot.chamberC).put("ptc_c",snapshot.ptcC)
                .put("target_c",snapshot.targetC).put("fan_on",snapshot.fanOn)
                .put("zc_present",snapshot.zeroCrossSignalPresent).put("heater_locked",snapshot.heaterLocked)
                .put("usage_available",snapshot.usageAvailable)
                .put("heater_ms",if(snapshot.usageAvailable) snapshot.heaterUsageMs else JSONObject.NULL)
                .put("filter_ms",if(snapshot.usageAvailable) snapshot.filterUsageMs else JSONObject.NULL).toString(2)
            json.launch("SnapHeater-report.json")
        }) { ActionLabel(Icons.Outlined.FileDownload, stringResource(R.string.history_report)) }
        if(status.isNotBlank()) Text(status)
    }
}

@Composable
internal fun TemperatureChart(points: List<TemperaturePoint>, gaps: List<HistoryGap>) {
    val colors=listOf(Color(0xFF35B6F2),Color(0xFFFFA144),Color(0xFF76CE9B))
    val temperatures=points.flatMap {listOfNotNull(it.chamber,it.ptc,it.target)}
    val maximum = maxOf(60.0, (temperatures.maxOrNull() ?: 55.0)+5).toFloat()
    val minimum = minOf(0.0, (temperatures.minOrNull() ?: 5.0)-5).toFloat()
    val span=(points.last().time-points.first().time).coerceAtLeast(1).toFloat()
    Text("${maximum.toInt()} °C", style=MaterialTheme.typography.labelSmall)
    Canvas(Modifier.fillMaxWidth().height(220.dp)) {
        for(i in 0..4) {
            val y=size.height*i/4
            drawLine(Color.Gray.copy(alpha=.25f),Offset(0f,y),Offset(size.width,y))
        }
        fun point(p:TemperaturePoint,value:Double)=Offset((p.time-points.first().time)/span*size.width,
            size.height*(1-(value.toFloat()-minimum)/(maximum-minimum)))
        gaps.filter {it.time in points.first().time..points.last().time}.forEach {
            val x=(it.time-points.first().time)/span*size.width
            drawLine(Color.Red.copy(alpha=.5f),Offset(x,0f),Offset(x,size.height),2f)
        }
        points.zipWithNext().forEach { (a,b) ->
            // Never interpolate an offline interval.
            if(b.time-a.time in 1..30_000 && a.segment==b.segment &&
                (a.segment=="phone" || b.sequence==a.sequence+1)) {
                if(a.chamber!=null && b.chamber!=null) drawLine(colors[0],point(a,a.chamber),point(b,b.chamber),3f)
                if(a.ptc!=null && b.ptc!=null) drawLine(colors[1],point(a,a.ptc),point(b,b.ptc),3f)
                if(a.target!=null && b.target!=null) drawLine(colors[2],point(a,a.target),point(b,b.target),2f)
            }
        }
    }
    Text("${minimum.toInt()} °C",style=MaterialTheme.typography.labelSmall)
    val format=SimpleDateFormat("HH:mm",Locale.getDefault())
    Row(Modifier.fillMaxWidth(), horizontalArrangement=Arrangement.SpaceBetween) {
        Text(format.format(Date(points.first().time))); Text(format.format(Date(points.last().time)))
    }
    Text(stringResource(R.string.history_chamber),color=colors[0])
    Text(stringResource(R.string.history_ptc),color=colors[1])
    Text(stringResource(R.string.history_target),color=colors[2])
}
