"""Source-level UI contract checks; not a substitute for on-device UI testing."""
from pathlib import Path
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / "apps/android/SnapHeaterU1/app/src/main"
UI = APP / "java/com/alphastudio/snapheateru1/ui"


class DailyUiTests(unittest.TestCase):
    def test_wifi_setup_precedes_printer_and_never_waits_forever(self):
        app = (UI / "SnapHeaterApp.kt").read_text(encoding="utf-8")
        self.assertLess(app.index("            WifiSetupScreen("), app.index("        PrinterSetupScreen("))
        self.assertIn("!snapshot.wifi.connected || !connectionHealthy", app)
        wifi = (UI / "screens/WifiSetupScreen.kt").read_text(encoding="utf-8")
        for key in ("wifi_auth_failed", "wifi_not_found", "wifi_timeout", "wifi_request_failed",
                    "wifi_result_unavailable", "wifi_try", "wifi_scan"):
            for locale in ("values", "values-pl", "values-de"):
                keys = {s.attrib.get("name") for s in ET.parse(APP / f"res/{locale}/strings.xml").getroot()}
                self.assertIn(key, keys)
        self.assertIn("seconds < 35", wifi)
        self.assertIn("waiting && !timedOut", wifi)
        self.assertIn("error.isBlank() && wifi.connected", wifi)
        self.assertNotIn("rememberSaveable { mutableStateOf(password", wifi)
        source = (ROOT / "main/wifi_sta.c").read_text(encoding="utf-8")
        self.assertIn("esp_wifi_set_storage(WIFI_STORAGE_RAM)", source)
        self.assertIn("#define SETUP_TIMEOUT_MS 15000", source)
        self.assertIn("shu1_control_network_setup_begin()", source)
        ble = (ROOT / "main/ble_control.c").read_text(encoding="utf-8")
        self.assertIn("g_ble_unlocked && dedicated && fresh", ble)

    def test_printer_discovery_is_bounded_and_read_only(self):
        discovery = (UI.parent / "data/PrinterDiscovery.kt").read_text(encoding="utf-8")
        for token in ('"_snapmaker._tcp."', '"_moonraker._tcp."', "140)", "= 900",
                      "withTimeoutOrNull(1200)", "withTimeoutOrNull(1800)",
                      "TRANSPORT_WIFI", "network.openConnection", "network.socketFactory",
                      "link.prefixLength", "currentCoroutineContext().ensureActive()",
                      "nsd.stopServiceDiscovery(listener)", "65536", "65_000_000_000L"):
            self.assertIn(token, discovery)
        self.assertIn('connection.requestMethod = "GET"', discovery)
        for forbidden in ('"POST"', "applySettings", "writeControl", "setTarget"):
            self.assertNotIn(forbidden, discovery)
        self.assertIn('"/printer/info"', discovery)
        self.assertIn("print_stats=state&heater_bed=temperature,target&webhooks=state", discovery)
        wizard = (UI / "screens/PrinterSetupScreen.kt").read_text(encoding="utf-8")
        selection = wizard.split("results.forEach", 1)[1].split("OutlinedTextField(host", 1)[0]
        self.assertIn("host = printer.host", selection)
        self.assertNotIn("onSave(", selection)
        for locale in ("values", "values-pl", "values-de"):
            keys = {s.attrib.get("name") for s in ET.parse(APP / f"res/{locale}/strings.xml").getroot()}
            self.assertTrue({"discovery_search", "discovery_help", "discovery_failed"} <= keys)

    def test_device_names_use_short_label_but_full_identity(self):
        app = (UI / "SnapHeaterApp.kt").read_text(encoding="utf-8")
        self.assertIn('"SH_${it.takeLast(4)}"', app)
        self.assertIn('"device_name_$stableId"', app)
        self.assertIn('putString("device_id_$canonical", stableId)', app)
        self.assertIn("previousId == null || previousId == stableId", app)
        self.assertIn("if (name.isBlank()) edit.remove(deviceNameKey(address))", app)
        connect = (UI / "screens/ConnectScreen.kt").read_text(encoding="utf-8")
        self.assertIn("onRenameDevice(it, newName)", connect)
        for locale in ("values", "values-pl", "values-de"):
            keys = {s.attrib.get("name") for s in ET.parse(APP / f"res/{locale}/strings.xml").getroot()}
            self.assertTrue({"device_rename", "device_name_help"} <= keys)

    def test_saved_devices_are_above_wizard_and_confirmed_only(self):
        connect = (UI / "screens/ConnectScreen.kt").read_text(encoding="utf-8")
        self.assertLess(connect.index("R.string.connect_saved_devices"), connect.index("R.string.wizard_snapheater"))
        self.assertIn("onSavedDevice(address)", connect)
        self.assertIn("enabled = !isConnecting && !isScanning", connect)
        app = (UI / "SnapHeaterApp.kt").read_text(encoding="utf-8")
        self.assertIn('putStringSet("confirmed_devices", savedDevices.toSet())', app)
        self.assertIn("(savedDevices + canonical).distinct().sorted()", app)
        scan = app.split("fun startBleScan()", 1)[1].split("val blePermissionLauncher", 1)[0]
        self.assertNotIn("rememberConnectedDevice", scan)
        saved_connect = app.split("fun connectSavedDevice(", 1)[1].split("fun requestSafeStop", 1)[0]
        self.assertLess(saved_connect.index(".onSuccess"), saved_connect.index("rememberConnectedDevice(address, latest.deviceId)"))
        self.assertIn("snapshot = latest", saved_connect)
        reset = app.split("fun rememberConnectedDevice(", 1)[1].split("fun connectSavedDevice", 1)[0]
        self.assertIn("printerSkipped = true", reset)
        self.assertIn("printerWizard = true", reset)

    def test_connection_wizard_requires_verified_printer_data(self):
        app = (UI / "SnapHeaterApp.kt").read_text(encoding="utf-8")
        self.assertIn("!printerSkipped && connectionHealthy && snapshot.printerDataReady", app)
        self.assertIn("onSkip = { printerSkipped = true; printerWizard = false }", app)
        self.assertIn("confirmed.mode.requiresPrinter() && !printerAllowed", app)
        self.assertLess(app.index("if (printerWizard)"), app.index("    SnapHeaterScaffold("))
        modes = (UI / "screens/ModesScreen.kt").read_text(encoding="utf-8")
        self.assertIn("enabled = !mode.requiresPrinter() || printerAllowed", modes)
        self.assertIn("!selected.requiresPrinter() || printerAllowed", modes)
        wizard = (UI / "screens/PrinterSetupScreen.kt").read_text(encoding="utf-8")
        wifi = (UI / "screens/WifiSetupScreen.kt").read_text(encoding="utf-8")
        self.assertIn('var password by remember { mutableStateOf("") }', wifi)
        self.assertIn("ready && !busy", wizard)
        self.assertIn('var apiKey by remember { mutableStateOf("") }', wizard)
        self.assertIn("connectionHealthy && snapshot.printerDataReady", app)
        for locale in ("values", "values-pl", "values-de"):
            keys = {s.attrib.get("name") for s in ET.parse(APP / f"res/{locale}/strings.xml").getroot()}
            self.assertTrue({"wizard_skip", "wizard_restart", "wizard_printer", "wizard_modes_locked"} <= keys)
        for source in ("main/api_server.c", "main/ble_control.c"):
            text = (ROOT / source).read_text(encoding="utf-8")
            self.assertIn("!shu1_device_config_restart_required()", text)
            self.assertIn("now_ms - st.printer.last_update_ms <= SHU1_PRINTER_STALE_MS", text)

    def test_auto_tempering_selection_and_transport_contract(self):
        modes = (UI / "screens/ModesScreen.kt").read_text(encoding="utf-8")
        self.assertIn("AppMode.AutoStandbyTempering, AppMode.ManualHold", modes)
        mapping = (UI.parent / "data/FirmwareModeMapping.kt").read_text(encoding="utf-8")
        self.assertIn('put("finish_conditioning_mode", if (temper) 2 else 1)', mapping)
        self.assertIn('put("tempering_enabled", temper)', mapping)
        self.assertIn('put("tempering_end_temp", 35)', mapping)
        for repo in ("BleSnapHeaterRepository.kt", "FirmwareSnapHeaterRepository.kt"):
            text = (UI.parent / "data" / repo).read_text(encoding="utf-8")
            self.assertIn("command(snapshot.jobPayload())", text)
            self.assertIn(".withAutoFinishPolicy(mode)", (UI.parent / "data/ControlPayloads.kt").read_text(encoding="utf-8"))
            self.assertIn("temperingEnabled =", text)
            self.assertIn("finishConditioningMode =", text)
        for locale in ("values", "values-pl", "values-de"):
            strings = ET.parse(APP / f"res/{locale}/strings.xml").getroot()
            keys = {s.attrib.get("name") for s in strings}
            self.assertTrue({"mode_auto_tempering", "mode_auto_tempering_detail"} <= keys)

    def test_stopped_dashboard_hides_target_without_erasing_settings(self):
        dashboard = (UI / "screens/DashboardScreen.kt").read_text(encoding="utf-8")
        self.assertIn("telemetryFresh && !stopped && !stopPending", dashboard)
        self.assertIn("R.string.heating_cooling", dashboard)
        self.assertIn("R.string.heating_stopping", dashboard)
        self.assertNotIn("targetC = 0", dashboard)
        mapping = (UI.parent / "data/FirmwareModeMapping.kt").read_text(encoding="utf-8")
        self.assertLess(mapping.index("workOn == false -> AppMode.SafeStop"),
                        mapping.index("preheatRunning ->"))

    def test_localized_context_keeps_activity_owner_chain(self):
        language = (UI / "AppLanguage.kt").read_text(encoding="utf-8")
        self.assertIn("object : ContextWrapper(context)", language)
        self.assertIn("override fun getResources()", language)
        self.assertNotIn("val localizedContext = remember(context, configuration) { context.createConfigurationContext(configuration) }", language)

    def test_language_defaults_to_english_and_does_not_reset_transport(self):
        language = (UI / "AppLanguage.kt").read_text(encoding="utf-8")
        self.assertIn('getString("language", "en")', language)
        self.assertIn('putString("language", selected)', language)
        for code in ('"en"', '"pl"', '"de"'):
            self.assertIn(code, language)
        app = (UI / "SnapHeaterApp.kt").read_text(encoding="utf-8")
        self.assertIn("remember(deviceContext, connectedBaseUrl)", app)

    def test_visual_preview_is_separate_from_device_transport(self):
        app = (UI / "SnapHeaterApp.kt").read_text(encoding="utf-8")
        self.assertLess(app.index("if (visualPreview &&"), app.index("val context ="))
        branch = app.split("if (visualPreview &&", 1)[1].split("val context =", 1)[0]
        self.assertIn("BuildConfig.ENABLE_VISUAL_PREVIEW", branch)
        self.assertIn("return", branch)
        preview = (UI / "screens/VisualPreviewScreen.kt").read_text(encoding="utf-8")
        for forbidden in ("import com.alphastudio.snapheateru1.data",
                          "import com.alphastudio.snapheateru1.ble",
                          "LaunchedEffect", "applySettings(", "applySafety(",
                          "getSharedPreferences", "connectedBaseUrl"):
            self.assertNotIn(forbidden, preview)
        self.assertIn("visual_preview_notice", preview)
        self.assertIn("BackHandler", preview)
        self.assertIn("AppTab.Dashboard, AppTab.Modes, AppTab.History, AppTab.Settings", preview)
        self.assertIn("AppTab.History -> PreviewHistoryScreen()", preview)
        history_preview = (UI / "screens/PreviewHistoryScreen.kt").read_text(encoding="utf-8")
        for forbidden in ("TemperatureHistory(", "EventHistory(", "LaunchedEffect", "CreateDocument", "Repository"):
            self.assertNotIn(forbidden, history_preview)
        self.assertIn("if (onPreview != null)", (UI / "screens/ConnectScreen.kt").read_text(encoding="utf-8"))

    def test_each_mode_has_a_shared_vector_icon(self):
        icons = (UI / "components/ActionIcons.kt").read_text(encoding="utf-8")
        for mode in ("Preheat", "ManualHold", "Drying", "AutoStandby", "AutoStandbyTempering", "Tempering", "SafeStop"):
            self.assertIn(f"AppMode.{mode} ->", icons)
        modes = (UI / "screens/ModesScreen.kt").read_text(encoding="utf-8")
        self.assertIn("ModeIcon(mode,", modes)
        self.assertIn("ActionLabel(selected,", modes)
        combined = icons.split("if (mode == AppMode.AutoStandbyTempering)", 1)[1].split("} else", 1)[0]
        self.assertLess(combined.index("Icon(ModeIcons.Printer"), combined.index("Icon(Icons.Outlined.Add"))
        self.assertLess(combined.index("Icon(Icons.Outlined.Add"), combined.index("Icon(ModeIcons.Cooldown"))
        self.assertNotIn("PrinterCooldown", icons)
        self.assertIn("contentDescription = null", icons)

    def test_no_manual_arming_requirement(self):
        app = (UI / "SnapHeaterApp.kt").read_text(encoding="utf-8")
        gate = app.split("val heatingAllowed =", 1)[1].split("val safetyWarning", 1)[0]
        self.assertNotIn("outputSafetyLatchReady", gate)
        self.assertIn("connectionHealthy", gate)
        self.assertIn("heaterOutputBuildEnabled", gate)
        safety = (UI / "screens/SafetyScreen.kt").read_text(encoding="utf-8")
        self.assertNotIn("Checkbox(", safety)
        self.assertIn("automatic_safety_help", safety)

    def test_three_primary_destinations_and_global_stop(self):
        text = (UI / "SnapHeaterApp.kt").read_text(encoding="utf-8")
        self.assertIn("listOf(AppTab.Dashboard, AppTab.Modes, AppTab.History, AppTab.Settings)", text)
        self.assertIn("TextButton(onClick = onSafeStop)", text)

    def test_dashboard_has_no_safety_percentage(self):
        text = (UI / "screens/DashboardScreen.kt").read_text(encoding="utf-8")
        self.assertNotIn("safetyScore", text)
        self.assertNotIn("Slider(", text)

    def test_mode_selection_is_a_local_draft(self):
        text = (UI / "screens/ModesScreen.kt").read_text(encoding="utf-8")
        self.assertIn("selectedName = mode.name", text)
        self.assertNotIn("onMode(mode)", text)
        self.assertNotIn("onSnapshotChange", text)
        self.assertIn("coerceIn(30, 55)", text)
        self.assertIn("enabled = heatingAllowed", text)

    def test_localized_daily_copy(self):
        keys = None
        for locale in ("values", "values-pl", "values-de"):
            root = ET.parse(APP / f"res/{locale}/strings.xml").getroot()
            current = {s.attrib["name"] for s in root if s.attrib.get("name", "").startswith("daily_")}
            self.assertGreater(len(current), 10)
            if keys is not None:
                self.assertEqual(keys, current)
            keys = current

    def test_settings_ceiling_matches_runtime_policy(self):
        text = (UI / "screens/SettingsScreen.kt").read_text(encoding="utf-8")
        self.assertNotIn("30f..70f", text)
        self.assertIn("30f..55f", text)
