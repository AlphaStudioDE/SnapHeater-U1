package com.alphastudio.snapheateru1.ui.screens

import androidx.compose.foundation.layout.*
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.FileDownload
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import com.alphastudio.snapheateru1.R
import com.alphastudio.snapheateru1.ui.components.ActionLabel
import com.alphastudio.snapheateru1.ui.components.ScreenColumn
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/** In-memory illustration only: no database, device, recorder or export access. */
@Composable
fun PreviewHistoryScreen() {
    val end = remember { System.currentTimeMillis() }
    var hours by rememberSaveable { mutableStateOf(2) }
    val points = remember(end, hours) { previewTemperaturePoints(end, hours) }
    ScreenColumn {
        Text(stringResource(R.string.history_title), style = MaterialTheme.typography.headlineMedium)
        Text(stringResource(R.string.preview_history_note), style = MaterialTheme.typography.bodySmall)
        Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            listOf(2, 24).forEach { range ->
                FilterChip(selected = hours == range, onClick = { hours = range }, label = { Text("$range h") })
            }
        }
        TemperatureChart(points, emptyList())
        Text(stringResource(R.string.error_history_title), style = MaterialTheme.typography.titleLarge)
        val format = SimpleDateFormat("dd.MM.yyyy HH:mm:ss", Locale.getDefault())
        listOf(
            Triple(43, R.string.freeze_event_ended, false),
            Triple(45, R.string.freeze_event_warning, true),
        ).forEach { (minutes, description, warning) ->
            OutlinedCard(Modifier.fillMaxWidth()) {
                Column(Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                    Text(format.format(Date(end - minutes * 60_000L)), style = MaterialTheme.typography.labelMedium)
                    Text(stringResource(description), color = if (warning) MaterialTheme.colorScheme.error
                        else MaterialTheme.colorScheme.onSurface)
                }
            }
        }
        Button(enabled = false, onClick = {}) {
            ActionLabel(Icons.Outlined.FileDownload, stringResource(R.string.history_export))
        }
        OutlinedButton(enabled = false, onClick = {}) {
            ActionLabel(Icons.Outlined.FileDownload, stringResource(R.string.history_report))
        }
    }
}
