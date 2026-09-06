/* SnapHeater U1 — Copyright (c) 2026 Damian Borkowski — SPDX-License-Identifier: MIT */
package com.alphastudio.snapheateru1.ui

import android.content.Context
import android.content.ContextWrapper
import android.content.res.Resources
import android.content.res.AssetManager
import android.content.res.Configuration
import android.os.LocaleList
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Box
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.Check
import androidx.compose.material.icons.outlined.Language
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.*
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.LocalContext
import com.alphastudio.snapheateru1.ui.components.ActionLabel
import java.util.Locale

private val languages = linkedMapOf("en" to "English", "pl" to "Polski", "de" to "Deutsch")
private data class LanguageChoice(val code: String, val select: (String) -> Unit)
private val LocalLanguageChoice = staticCompositionLocalOf { LanguageChoice("en") {} }

@Composable
fun AppLanguageProvider(context: Context, content: @Composable () -> Unit) {
    val preferences = remember(context) { context.getSharedPreferences("app_language", Context.MODE_PRIVATE) }
    var language by remember {
        mutableStateOf(preferences.getString("language", "en").takeIf { it in languages } ?: "en")
    }
    val systemConfiguration = LocalConfiguration.current
    val configuration = remember(systemConfiguration, language) {
        Configuration(systemConfiguration).apply {
            setLocales(LocaleList.forLanguageTags(language))
            setLayoutDirection(Locale.forLanguageTag(language))
        }
    }
    val localizedContext = remember(context, configuration) {
        val localizedResources = context.createConfigurationContext(configuration)
        // Keep the real Activity in the base-context chain. Compose discovers
        // ActivityResultRegistryOwner and back dispatch through this chain.
        // A bare configuration context loses those owners and crashes startup.
        object : ContextWrapper(context) {
            override fun getResources(): Resources = localizedResources.resources
            override fun getAssets(): AssetManager = localizedResources.assets
        }
    }
    CompositionLocalProvider(
        LocalContext provides localizedContext,
        LocalConfiguration provides configuration,
        LocalLanguageChoice provides LanguageChoice(language) { selected ->
            if (selected in languages) {
                preferences.edit().putString("language", selected).apply()
                language = selected
            }
        },
        content = content,
    )
}

@Composable
fun LanguagePicker() {
    val choice = LocalLanguageChoice.current
    var expanded by remember { mutableStateOf(false) }
    Column {
        Text(when (choice.code) { "pl" -> "Język aplikacji"; "de" -> "App-Sprache"; else -> "App language" })
        Box {
            TextButton(onClick = { expanded = true }) {
                ActionLabel(Icons.Outlined.Language, languages.getValue(choice.code))
            }
            DropdownMenu(expanded = expanded, onDismissRequest = { expanded = false }) {
                languages.forEach { (code, name) ->
                    DropdownMenuItem(
                        text = { ActionLabel(if (choice.code == code) Icons.Outlined.Check else Icons.Outlined.Language, name) },
                        onClick = { choice.select(code); expanded = false },
                    )
                }
            }
        }
    }
}
