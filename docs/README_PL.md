# SnapHeater U1 0.9.9

**Twoja grzałka komory. Połączona z Twoim wydrukiem.**

Otwarte firmware dla oryginalnej elektroniki Panda Breath, aplikacja Android
i bezpośrednia integracja z drukarką Snapmaker U1 przez Moonraker.
Nie jest to firmware dla AirGuard 300.

> **Wersja testowa, nie stabilna.** Testujesz na własną odpowiedzialność.
> Należy stale nadzorować urządzenie i zgłaszać nieprawidłowości. Oprogramowanie
> udostępniamy bez gwarancji; autorzy wyłączają odpowiedzialność za szkody
> w zakresie dopuszczalnym przez obowiązujące prawo. Nie jest to zapewnienie
> bezpieczeństwa urządzenia ani zastępstwo jego prawidłowych zabezpieczeń.

## Co zyskujesz

- Czytelne tryby: preheat, AUTO, AUTO + Tempering, manual hold i suszenie.
- Pauzę grzania zachowującą zadanie oraz osobne zatrzymanie.
- Kreator połączenia z grzałką, Wi-Fi i drukarką; listę nazwanych grzałek.
- Wykresy, historię zdarzeń, eksport CSV i raport diagnostyczny.
- Uzupełnianie historii z bufora Pandy: próbka co 10 s, okno do 120 minut.
- Opcjonalny Symbiont sterujący obsługiwaną wentylacją komory/top cover.
- Lokalną realizację zadania na Pandzie mimo utraty połączenia z telefonem.
- Aktualizację OTA przez LAN do nieaktywnej partycji ze sprawdzeniem SHA-256.

Normalne uruchomienie trybu nie wymaga osobnego rytuału uzbrajania. Sam start
urządzenia nie włącza grzania. Zabezpieczenia temperatury, czujników i ZC pozostają
aktywne; po awarii nie ma automatycznego wznowienia zadania.
Limit celu wynosi 55 °C i nie oznacza gwarancji osiągnięcia tej temperatury.

Nie obiecujemy potwierdzonej poprawy jakości wydruków, oszczędności energii ani
większego bezpieczeństwa niż stock — takie porównania wymagają pomiarów.
Powiadomienia telefonu nie są gwarantowane i nie zastępują nadzoru.

[Pobierz wydanie](https://github.com/AlphaStudioDE/SnapHeater-U1/releases/tag/v0.9.9) ·
[Instrukcja instalacji / OTA](INSTALL_0.9.9.md) · [Stan funkcji](../FEATURE_MATRIX.md) ·
[Galeria](GALLERY.md) · [Opis projektu i Innovation Fund](PROJECT_STORY.md).

Projekt został reanimowany dzięki odkryciom **plastikmana, autora
[DragonBreath](https://github.com/plastikman/DragonBreath)**.
SnapHeater pozostaje niezależnym projektem z własną aplikacją i funkcjami U1.
