#pragma once
// Minimale, abhängigkeitsfreie Bild-Typen für den Erkennungskern.

#include <algorithm>
#include <cstdint>

namespace hb {

// Ein Bild im Speicher: 4 Byte pro Pixel (BGRA oder RGBA, die Reihenfolge ist egal,
// weil die Erkennung nur den hellsten Farbkanal pro Pixel verwendet).
struct FrameView {
	const uint8_t *data = nullptr;
	int width = 0;
	int height = 0;
	int stride = 0; // Bytes pro Zeile

	bool valid() const { return data && width > 0 && height > 0 && stride >= width * 4; }
};

// Rechteck in normierten Koordinaten (0..1), unabhängig von der Kameraauflösung.
struct NormRect {
	double x = 0.0;
	double y = 0.0;
	double w = 0.0;
	double h = 0.0;

	bool valid() const { return w > 0.002 && h > 0.002; }

	NormRect clamped() const
	{
		NormRect r;
		r.x = std::clamp(x, 0.0, 1.0);
		r.y = std::clamp(y, 0.0, 1.0);
		r.w = std::clamp(x + w, 0.0, 1.0) - r.x;
		r.h = std::clamp(y + h, 0.0, 1.0) - r.y;
		return r;
	}
};

} // namespace hb
