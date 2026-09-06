package com.alphastudio.snapheateru1.ui.components

import androidx.compose.runtime.*
import androidx.compose.material3.*
import androidx.compose.foundation.layout.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import com.alphastudio.snapheateru1.R

@Composable
fun RestAccessCard(enabled: Boolean, onProvision: (String) -> Unit) {
    var token by remember { mutableStateOf("") }
    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Text(stringResource(R.string.rest_setup))
        OutlinedTextField(value = token, onValueChange = { token = it.take(64) },
            label = { Text(stringResource(R.string.rest_token)) }, singleLine = true,
            visualTransformation = PasswordVisualTransformation())
        Text(stringResource(R.string.rest_setup_note))
        Button(enabled = enabled && token.length in 16..64, onClick = { onProvision(token); token = "" }) {
            Text(stringResource(R.string.rest_save))
        }
    }
}
