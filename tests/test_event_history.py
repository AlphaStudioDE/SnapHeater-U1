"""Production Kotlin SQL on desktop SQLite, plus UI/transport contract checks.
Not Android instrumentation or a notification-delivery guarantee.
"""
import re
import sqlite3
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
APP=ROOT/"apps/android/SnapHeaterU1/app/src/main/java/com/alphastudio/snapheateru1"

class EventHistoryTests(unittest.TestCase):
    def test_deduplication_device_boot_separation_and_date_order(self):
        source=(APP/"data/EventHistory.kt").read_text(encoding="utf-8")
        with sqlite3.connect(":memory:") as db:
            for sql in re.findall(r'db.execSQL\("(CREATE [^"]+)"\)',source): db.execute(sql)
            for row in [("A","boot1","1",100,"warning","warn",1),
                        ("A","boot1","1",999,"warning","warn",1),
                        ("A","boot2","1",200,"fault","critical",1),
                        ("B","boot1","1",300,"fault","critical",1),
                        ("A","phone","action",150,"continue","info",0)]:
                db.execute("INSERT OR IGNORE INTO events VALUES (?,?,?,?,?,?,?)",row)
            query=re.search(r'rawQuery\("(SELECT time,code,level,approximate[^\"]+)"',source)[1]
            rows=db.execute(query,("A",100)).fetchall()
            self.assertEqual([r[0] for r in rows],[200,150,100])
            self.assertEqual(len(db.execute(query,("A",2)).fetchall()),2)
            self.assertEqual(len(db.execute(query,("B",100)).fetchall()),1)

    def test_history_not_gated_on_notification_permission_and_continue_is_local(self):
        source=(APP/"data/DeviceEvents.kt").read_text(encoding="utf-8")
        self.assertLess(source.index("it.ingest(expectedDevice,page)"),source.index("areNotificationsEnabled"))
        app=(APP/"ui/SnapHeaterApp.kt").read_text(encoding="utf-8")
        block=app.split('dismissButton = { TextButton(onClick={',1)[1].split('R.string.freeze_continue',1)[0]
        self.assertIn('"phone_freeze_continue"',block)
        self.assertNotIn("repository",block)
        self.assertNotIn("applySettings",block)
        self.assertIn("requestSafeStop(true)",app)
        self.assertIn("latest.deviceId.equals(expectedId,true)",app)
        chart=(APP/"ui/screens/HistoryScreen.kt").read_text(encoding="utf-8")
        self.assertLess(chart.index("else TemperatureChart(points,gaps)"),chart.index("R.string.error_history_title"))
        self.assertIn('"dd.MM.yyyy HH:mm:ss"',chart)
        self.assertIn("eventLimit+=100",chart)

    def test_firmware_warning_is_not_a_sensor_fault_and_has_fixed_grace(self):
        source=(ROOT/"main/safety.c").read_text(encoding="utf-8")
        self.assertIn("sample_health != SHU1_SAMPLE_HEALTHY && sample_health != SHU1_SAMPLE_WARNING",source)
        self.assertIn('"sensor_freeze_warning"',source)
        monitor=(ROOT/"main/sensor_watch.h").read_text(encoding="utf-8")
        self.assertIn("SHU1_RAW_WARNING_GRACE_US INT64_C(300000000)",monitor)
        self.assertNotIn("acknowledge",monitor.split("typedef struct",1)[1].split("} shu1_sensor_watch_t",1)[0])

if __name__=="__main__": unittest.main()
