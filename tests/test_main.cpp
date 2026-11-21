// Einfache Tests ohne Framework: `ctest` bzw. direkt ausführen.

#include "../src/core/match_tracker.hpp"
#include "../src/core/seven_segment.hpp"
#include "synthetic_board.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                                          \
	do {                                                                                 \
		++g_checks;                                                                  \
		if (!(cond)) {                                                               \
			++g_failures;                                                        \
			std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
		}                                                                            \
	} while (0)

using namespace hb;

// --- Erkennung -------------------------------------------------------------------------------

struct Scenario {
	const char *name;
	synth::Style style;
	double H; // Ziffernhöhe in Pixeln
	double minRate; // geforderte Trefferquote
};

static double runScenario(const Scenario &sc, int digits)
{
	int total = 0, ok = 0;
	const int maxVal = digits == 1 ? 9 : digits == 2 ? 99 : 999;
	for (int value = 0; value <= maxVal; value += (digits == 3 ? 7 : 1)) {
		const double pitch = sc.H * sc.style.widthFrac * 1.35;
		synth::Image img(int(pitch * digits + 120), int(sc.H + 80), sc.style);
		const double x = 60, y = 40;
		synth::drawNumber(img, value, digits, x, y, sc.H, pitch, sc.style);
		synth::finish(img, sc.style, unsigned(value + 17));
		const NormRect r = synth::rectAround(img, x, y, sc.H, pitch, digits, 8);
		const FieldReading res = SevenSegmentReader::read(img.view(), r, digits);
		++total;
		if (res.status == ReadStatus::Ok && res.value == value)
			++ok;
		else if (std::getenv("HB_VERBOSE"))
			std::printf("    %s digits=%d value=%d -> status=%d value=%d text='%s'\n", sc.name, digits,
				    value, int(res.status), res.value, res.text.c_str());
	}
	return double(ok) / double(total);
}

static void testReader()
{
	std::printf("[Erkennung]\n");
	std::vector<Scenario> scenarios;
	{
		synth::Style s;
		scenarios.push_back({"sauber", s, 60, 1.0});
	}
	{
		synth::Style s;
		scenarios.push_back({"klein (28px)", s, 28, 1.0});
	}
	{
		synth::Style s;
		s.noise = 18;
		scenarios.push_back({"Rauschen", s, 50, 0.99});
	}
	{
		synth::Style s;
		s.blur = 1;
		s.noise = 10;
		scenarios.push_back({"unscharf", s, 50, 0.99});
	}
	{
		synth::Style s;
		s.bloom = 1;
		scenarios.push_back({"Leuchtrand", s, 60, 0.99});
	}
	{
		synth::Style s;
		s.ghost = 50;
		scenarios.push_back({"Geisterbild", s, 60, 1.0});
	}
	{
		synth::Style s;
		s.litR = 255, s.litG = 255, s.litB = 255;
		s.bgR = 0, s.bgG = 0, s.bgB = 40;
		scenarios.push_back({"weiss auf blau", s, 50, 1.0});
	}
	{
		synth::Style s;
		s.litR = 15, s.litG = 15, s.litB = 15;
		s.bgR = 190, s.bgG = 200, s.bgB = 190;
		scenarios.push_back({"dunkel auf hell (LCD)", s, 50, 1.0});
	}
	{
		synth::Style s;
		s.litR = 20, s.litG = 230, s.litB = 40;
		scenarios.push_back({"gruen", s, 50, 1.0});
	}
	{
		synth::Style s;
		s.strokeFrac = 0.18;
		scenarios.push_back({"dicke Striche", s, 50, 1.0});
	}

	for (const Scenario &sc : scenarios) {
		for (int digits = 1; digits <= 3; ++digits) {
			const double rate = runScenario(sc, digits);
			std::printf("  %-24s %d-stellig: %5.1f%%\n", sc.name, digits, rate * 100.0);
			CHECK(rate >= sc.minRate);
		}
	}

	// Kursive Ziffern brauchen den Schrägstellungs-Parameter.
	{
		synth::Style s;
		s.slant = 0.14;
		int ok = 0, total = 0;
		for (int value = 0; value <= 99; ++value) {
			const double H = 60, pitch = H * s.widthFrac * 1.45;
			synth::Image img(int(pitch * 2 + 140), int(H + 80), s);
			synth::drawNumber(img, value, 2, 70, 40, H, pitch, s);
			synth::finish(img, s);
			SegmentOptions o;
			o.slant = 0.14;
			const FieldReading r = SevenSegmentReader::read(img.view(), synth::rectAround(img, 70, 40, H, pitch, 2, 10), 2, o);
			++total;
			ok += (r.status == ReadStatus::Ok && r.value == value);
		}
		std::printf("  %-24s 2-stellig: %5.1f%%\n", "kursiv (slant=0.14)", 100.0 * ok / total);
		CHECK(ok >= total * 0.98);
	}

	// Leere Tafel und reines Rauschen dürfen keine Zahl ergeben.
	{
		synth::Style s;
		s.noise = 12;
		synth::Image img(300, 150, s);
		synth::finish(img, s);
		const FieldReading r = SevenSegmentReader::read(img.view(), NormRect{0.1, 0.1, 0.8, 0.8}, 2);
		CHECK(r.status != ReadStatus::Ok);
	}
	// Führende Leerstelle: "5" in einem zweistelligen Feld
	{
		synth::Style s;
		const double H = 60, pitch = H * s.widthFrac * 1.4;
		synth::Image img(int(pitch * 2 + 140), int(H + 80), s);
		synth::drawNumber(img, 5, 2, 70, 40, H, pitch, s);
		const FieldReading r = SevenSegmentReader::read(img.view(), synth::rectAround(img, 70, 40, H, pitch, 2, 8), 2);
		CHECK(r.status == ReadStatus::Ok && r.value == 5);
		CHECK(r.text == " 5");
	}
	// Ein Doppelpunkt direkt neben den Ziffern (Rahmen ist etwas zu groß) darf nicht stören.
	{
		synth::Style s;
		int ok = 0, total = 0;
		for (int value = 0; value <= 99; ++value) {
			const double H = 70, pitch = H * s.widthFrac * 1.35;
			synth::Image img(int(pitch * 2 + 200), int(H + 100), s);
			synth::drawNumber(img, value, 2, 60, 50, H, pitch, s);
			for (double cy : {50 + H * 0.3, 50 + H * 0.7})
				for (int dy = -3; dy <= 3; ++dy)
					for (int dx = -3; dx <= 3; ++dx)
						img.set(int(60 + 2 * pitch + 10 + dx), int(cy + dy), s.litR, s.litG, s.litB);
			synth::finish(img, s);
			// Rahmen reicht bis über den Doppelpunkt hinaus
			hb::NormRect r = synth::rectAround(img, 60, 50, H, pitch, 2, 8);
			r.w += 22.0 / img.w;
			const FieldReading res = SevenSegmentReader::read(img.view(), r, 2);
			++total;
			ok += (res.status == ReadStatus::Ok && res.value == value);
		}
		std::printf("  %-24s 2-stellig: %5.1f%%\n", "Doppelpunkt im Rahmen", 100.0 * ok / total);
		CHECK(ok == total);
	}
	// Ungültiges Muster ("Ziffer, Lücke, Ziffer") ist unlesbar statt falsch.
	{
		synth::Style s;
		const double H = 60, pitch = H * s.widthFrac * 1.4;
		synth::Image img(int(pitch * 3 + 140), int(H + 80), s);
		synth::drawDigit(img, 1, 70 + (pitch - H * s.widthFrac) / 2, 40, H, s);
		synth::drawDigit(img, 2, 70 + 2 * pitch + (pitch - H * s.widthFrac) / 2, 40, H, s);
		const FieldReading r = SevenSegmentReader::read(img.view(), synth::rectAround(img, 70, 40, H, pitch, 3, 8), 3);
		CHECK(r.status != ReadStatus::Ok);
	}
}

// --- Tracker ---------------------------------------------------------------------------------

static Observation obs(double t, std::optional<int> h, std::optional<int> a, std::optional<int> min = {},
		       std::optional<int> sec = {}, std::optional<int> half = {})
{
	Observation o;
	o.time = t;
	o.home = h;
	o.away = a;
	o.minutes = min;
	o.seconds = sec;
	o.half = half;
	o.anyFieldVisible = true;
	return o;
}

static void testTracker()
{
	std::printf("[Tracker]\n");

	// Einzelner Ausreißer wird ignoriert, ein stabiles Tor übernommen.
	{
		MatchTracker t;
		double now = 0;
		for (int i = 0; i < 4; ++i)
			t.update(obs(now += 0.25, 3, 2));
		CHECK(t.state().home == 3 && t.state().away == 2);
		t.update(obs(now += 0.25, 8, 2)); // Lesefehler
		t.update(obs(now += 0.25, 3, 2));
		CHECK(t.state().home == 3);
		for (int i = 0; i < 3; ++i)
			t.update(obs(now += 0.25, 4, 2));
		CHECK(t.state().home == 4);
	}
	// Sinkender Wert braucht lange Bestätigung (Schiedsrichter-Korrektur).
	{
		MatchTracker t;
		double now = 0;
		for (int i = 0; i < 4; ++i)
			t.update(obs(now += 0.25, 10, 9));
		for (int i = 0; i < 5; ++i)
			t.update(obs(now += 0.25, 9, 9));
		CHECK(t.state().home == 10);
		for (int i = 0; i < 6; ++i)
			t.update(obs(now += 0.25, 9, 9));
		CHECK(t.state().home == 9);
	}
	// Erste Lesung beider Seiten ist schnell, auch wenn sie nicht gleichzeitig kommt.
	{
		MatchTracker t;
		double now = 0;
		for (int i = 0; i < 3; ++i)
			t.update(obs(now += 0.25, 12, {}));
		for (int i = 0; i < 3; ++i)
			t.update(obs(now += 0.25, 12, 11));
		CHECK(t.state().home == 12 && t.state().away == 11);
	}
	// Uhr, laufend-Erkennung und Halbzeit aus Zeitsprung (30-Minuten-Halbzeit, Uhr zählt hoch).
	{
		MatchTracker t;
		double now = 0;
		int sec = 0;
		for (int i = 0; i < 40; ++i) { // 10 s gespielt, 4 Lesungen pro Sekunde
			sec = i / 4;
			t.update(obs(now += 0.25, 0, 0, sec / 60, sec % 60));
		}
		CHECK(t.state().clockKnown && t.state().clockSeconds >= 8);
		CHECK(t.state().clockRunning);
		CHECK(t.state().half == 1);
		// Die Uhr läuft die ganze Halbzeit durch bis 30:00
		for (int s = 10; s <= 30 * 60; ++s)
			for (int k = 0; k < 4; ++k)
				t.update(obs(now += 0.25, 0, 0, s / 60, s % 60));
		CHECK(t.state().half == 1);
		CHECK(t.state().clockSeconds == 1800);
		// Pause: Uhr steht
		for (int k = 0; k < 20; ++k)
			t.update(obs(now += 0.25, 0, 0, 30, 0));
		CHECK(!t.state().clockRunning);
		// Uhr auf 00:00 zurückgesetzt -> 2. Halbzeit
		for (int k = 0; k < 14; ++k)
			t.update(obs(now += 0.25, 0, 0, 0, 0));
		CHECK(t.state().half == 2);
	}
	// Ohne Vorspiel (Uhr gerade erst gesetzt) darf ein Uhrsprung keine Halbzeit auslösen.
	{
		MatchTracker t;
		double now = 0;
		for (int k = 0; k < 8; ++k)
			t.update(obs(now += 0.25, {}, {}, 2, 0));
		for (int k = 0; k < 14; ++k)
			t.update(obs(now += 0.25, {}, {}, 25, 0));
		CHECK(t.state().clockSeconds == 25 * 60);
		CHECK(t.state().half == 1);
	}
	// Echtes Halbzeitfeld hat Vorrang vor der Ableitung.
	{
		MatchTracker t;
		double now = 0;
		for (int k = 0; k < 6; ++k)
			t.update(obs(now += 0.25, 5, 5, 10, 0, 1));
		for (int k = 0; k < 6; ++k)
			t.update(obs(now += 0.25, 5, 5, 10, 0, 2));
		CHECK(t.state().half == 2);
	}
	// Countdown-Uhr (30:00 -> 00:00, dann wieder 30:00).
	{
		MatchTracker t;
		double now = 0;
		for (int s = 30 * 60; s >= 0; --s)
			for (int k = 0; k < 4; ++k)
				t.update(obs(now += 0.25, 0, 0, s / 60, s % 60));
		for (int k = 0; k < 14; ++k)
			t.update(obs(now += 0.25, 0, 0, 30, 0));
		CHECK(t.state().half == 2);
	}
	// Signalverlust und manueller Modus.
	{
		MatchTracker t;
		double now = 0;
		for (int i = 0; i < 4; ++i)
			t.update(obs(now += 0.25, 1, 0));
		CHECK(t.state().signal);
		t.tick(now + 10);
		CHECK(!t.state().signal);
		t.setScore(7, 6);
		t.setScoreAutomatic(false);
		for (int i = 0; i < 20; ++i)
			t.update(obs(now += 0.25, 1, 0));
		CHECK(t.state().home == 7 && t.state().away == 6);
		t.setHalf(2);
		CHECK(t.state().half == 2);
	}
}

int main()
{
	testReader();
	testTracker();
	std::printf("\n%d Pruefungen, %d fehlgeschlagen\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
