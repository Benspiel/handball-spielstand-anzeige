#pragma once
// Erkennung von 7-Segment-Anzeigen (typische LED-Hallenanzeigetafeln).
//
// Ablauf pro Feld (z.B. "Heim-Tore"):
//   1. Bereich ausschneiden, pro Pixel den hellsten Farbkanal nehmen
//   2. Per Otsu-Schwelle in "leuchtet" / "leuchtet nicht" trennen
//      (leuchtend = die kleinere Pixelgruppe, funktioniert für helle Ziffern auf dunklem
//       Grund und umgekehrt)
//   3. Bereich vertikal auf die leuchtenden Zeilen zuschneiden
//   4. Über das Spaltenprofil die einzelnen Ziffern finden (das Rechteck darf also großzügig sein)
//   5. Pro Ziffer die 7 Segmentzonen auswerten, Segmentmuster -> Ziffer

#include "frame.hpp"

#include <string>

namespace hb {

enum class ReadStatus {
	Ok,         // Zahl gelesen
	Blank,      // nichts leuchtet (oder zu wenig Kontrast)
	Unreadable, // etwas leuchtet, ergibt aber keine gültige Ziffernfolge
};

enum class Polarity { Auto, BrightOnDark, DarkOnBright };

struct SegmentOptions {
	Polarity polarity = Polarity::Auto;
	// Anteil leuchtender Pixel in einer Segmentzone, ab dem das Segment als "an" gilt.
	double segmentThreshold = 0.40;
	// Mindestunterschied zwischen hell und dunkel (0..255), sonst gilt das Feld als leer.
	double minContrast = 40.0;
	// Schrägstellung kursiver Ziffern: Verschiebung der Oberkante nach rechts, relativ zur Ziffernbreite
	// (0 = aufrecht, typisch 0.1 bis 0.2 bei kursiven Tafeln).
	double slant = 0.0;
};

struct FieldReading {
	ReadStatus status = ReadStatus::Blank;
	int value = 0;           // nur gültig bei Status Ok
	double confidence = 0.0; // 0..1, wie eindeutig alle Segmente waren
	std::string text;        // erkannte Zeichen, links mit ' ' aufgefüllt, '?' = unlesbar (zum Debuggen)
};

class SevenSegmentReader {
public:
	// Liest eine Zahl mit `digits` Ziffern aus dem Rechteck `rect`.
	// Führende Leerstellen sind erlaubt ("  5" -> 5).
	static FieldReading read(const FrameView &frame, const NormRect &rect, int digits,
				 const SegmentOptions &options = SegmentOptions());
};

} // namespace hb
