package com.alphastudio.snapheateru1.ui.screens

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.SystemUpdate
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import com.alphastudio.snapheateru1.R
import com.alphastudio.snapheateru1.data.RestCredentials
import com.alphastudio.snapheateru1.data.SnapHeaterApiClient
import com.alphastudio.snapheateru1.model.HeaterSnapshot
import com.alphastudio.snapheateru1.model.AppMode
import com.alphastudio.snapheateru1.ui.components.ActionLabel
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

@Composable
fun OtaCard(address: String, snapshot: HeaterSnapshot, healthy: Boolean, busy: Boolean, onBusy: (Boolean)->Unit) {
    val context=LocalContext.current
    val scope=rememberCoroutineScope()
    var image by remember(address) { mutableStateOf<ByteArray?>(null) }
    var status by remember(address) { mutableStateOf("") }
    var progress by remember(address) { mutableStateOf(0f) }
    var uploading by remember { mutableStateOf(false) }
    val errorText=stringResource(R.string.ota_failed)
    val acceptedText=stringResource(R.string.ota_accepted)
    val eligible=healthy && !busy && !uploading && snapshot.mode==AppMode.SafeStop &&
        !snapshot.fanOn && address.isNotBlank() && !address.startsWith("ble://")
    val picker=rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if(uri!=null) scope.launch {
            onBusy(true)
            runCatching { withContext(Dispatchers.IO) {
                context.contentResolver.openInputStream(uri)?.use { input ->
                    val result=java.io.ByteArrayOutputStream()
                    val buffer=ByteArray(8192)
                    while(true) {
                        val n=input.read(buffer); if(n<0) break
                        require(result.size()+n<=2*1024*1024) { "Image exceeds 2 MiB" }
                        result.write(buffer,0,n)
                    }
                    result.toByteArray().also { require(it.size>32 && (it[0].toInt() and 255)==0xE9) { "Not an ESP application image" } }
                } ?: error("Cannot open file")
            } }.onSuccess { image=it; status="" }.onFailure { status=errorText }
            onBusy(false)
        }
    }
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement=Arrangement.spacedBy(8.dp)) {
            Text(stringResource(R.string.ota_title),style=MaterialTheme.typography.titleMedium)
            Text(stringResource(R.string.ota_note))
            Button(enabled=eligible,onClick={ picker.launch(arrayOf("application/octet-stream","application/macbinary","*/*")) }) {
                ActionLabel(Icons.Outlined.SystemUpdate,stringResource(R.string.ota_choose))
            }
            if(uploading) LinearProgressIndicator(progress={progress},modifier=Modifier.fillMaxWidth())
            if(status.isNotBlank()) Text(status)
        }
    }
    if(image!=null) AlertDialog(
        onDismissRequest={if(!uploading) image=null},
        title={Text(stringResource(R.string.ota_title))},
        text={Text(stringResource(R.string.ota_confirm,image!!.size))},
        dismissButton={TextButton(enabled=!uploading,onClick={image=null}){Text(stringResource(android.R.string.cancel))}},
        confirmButton={TextButton(enabled=eligible,onClick={
            val selected=image ?: return@TextButton
            val identity=snapshot.deviceId
            uploading=true; onBusy(true); image=null
            scope.launch {
                try {
                    val token=RestCredentials(context.applicationContext).get(address)
                    val reply=withContext(Dispatchers.IO) {
                        SnapHeaterApiClient(address,token).uploadFirmware(selected,identity) { value ->
                            scope.launch { progress=value }
                        }
                    }
                    status=acceptedText+" "+reply.optString("version")
                } catch (_: Exception) { status=errorText }
                finally { uploading=false; onBusy(false) }
            }
        }) {Text(stringResource(R.string.ota_upload))}}
    )
}
