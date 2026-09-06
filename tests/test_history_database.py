"""Execute the production Kotlin SQL with desktop SQLite (not Android instrumentation)."""
import re
import sqlite3
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
DATA=ROOT/"apps/android/SnapHeaterU1/app/src/main/java/com/alphastudio/snapheateru1/data"


class HistoryDatabaseTests(unittest.TestCase):
    def setUp(self):
        self.source=(DATA/"TemperatureHistory.kt").read_text(encoding="utf-8")
        self.db=sqlite3.connect(":memory:")
        for sql in re.findall(r'db.execSQL\("(CREATE [^"]+)"\)',self.source):
            self.db.execute(sql)
        self.combined=re.search(r'private val combined = """(.*?)"""',self.source,re.S)[1].strip()

    def tearDown(self):
        self.db.close()

    def sample(self, seq=1, device="AABBCCDDEEFF", boot="0123456789abcdef"):
        self.db.execute("INSERT OR IGNORE INTO device_samples VALUES (?,?,?,?,?,?,?,?,?,?)",
                        (device,boot,seq,seq*10000,35.1,42.2,45,0,0,0))

    def test_replay_deduplicates_but_devices_and_boots_do_not_collide(self):
        self.sample(); self.sample()
        self.sample(boot="fedcba9876543210")
        self.sample(device="112233445566")
        self.assertEqual(self.db.execute("SELECT COUNT(*) FROM device_samples").fetchone()[0],3)
        rows=self.db.execute(self.combined,("AABBCCDDEEFF",)*2).fetchall()
        self.assertEqual(len(rows),2)

    def test_failed_transaction_does_not_advance_cursor_or_keep_half_page(self):
        self.db.execute("INSERT INTO history_cursor VALUES (?,?,?)",("AABBCCDDEEFF","0123456789abcdef",0))
        self.db.commit()
        try:
            with self.db:
                self.sample()
                self.db.execute("UPDATE history_cursor SET seq=1")
                raise RuntimeError("simulated storage failure")
        except RuntimeError:
            pass
        self.assertEqual(self.db.execute("SELECT seq FROM history_cursor").fetchone()[0],0)
        self.assertEqual(self.db.execute("SELECT COUNT(*) FROM device_samples").fetchone()[0],0)

    def test_legacy_rows_preserved_but_not_duplicated_in_synchronized_coverage(self):
        self.db.execute("INSERT INTO samples VALUES (?,?,?,?,?,?)",("AABBCCDDEEFF",10000,35,42,45,0))
        self.db.execute("INSERT INTO samples VALUES (?,?,?,?,?,?)",("AABBCCDDEEFF",-50000,25,30,45,0))
        self.sample()
        self.db.execute("INSERT INTO history_boots VALUES (?,?,?,?,?)",
                        ("AABBCCDDEEFF","0123456789abcdef",0,10000,10000))
        self.assertEqual(self.db.execute("SELECT COUNT(*) FROM samples").fetchone()[0],2)
        self.assertEqual(len(self.db.execute(self.combined,("AABBCCDDEEFF",)*2).fetchall()),2)

    def test_real_export_query_keeps_nulls_and_gap_records(self):
        self.sample()
        self.db.execute("UPDATE device_samples SET chamber=NULL")
        self.db.execute("INSERT INTO history_gaps VALUES (?,?,?,?,?,?)",
                        ("AABBCCDDEEFF","0123456789abcdef",1,10000,"overwritten",10))
        measurements=re.search(r'val measurements="([^"]+)"',self.source)[1].replace("$combined",self.combined)
        gaps=re.search(r'val gaps="([^"]+)"',self.source)[1]
        rows=self.db.execute(measurements+" UNION ALL "+gaps+" ORDER BY time",("AABBCCDDEEFF",)*3).fetchall()
        self.assertEqual(len(rows),2)
        self.assertIsNone(rows[0][2])
        self.assertEqual(rows[1][0],"gap")
        self.assertEqual(rows[1][-2:],("overwritten",10))

    def test_ingest_transaction_wraps_rows_and_cursor(self):
        ingest=self.source.split("@Synchronized fun ingest",1)[1].split("/** Compatibility",1)[0]
        self.assertLess(ingest.index("db.beginTransaction()"),ingest.index('db.insertWithOnConflict("device_samples"'))
        self.assertLess(ingest.index('db.insertWithOnConflict("history_cursor"'),ingest.index("db.setTransactionSuccessful()"))
        self.assertIn("finally { db.endTransaction() }",ingest)
        self.assertIn("check(cursor(page.device)==expected)",ingest)
