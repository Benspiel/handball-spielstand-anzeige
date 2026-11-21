#pragma once
// Zeichnet künstliche 7-Segment-Anzeigen für die Tests (und für Demo-Bilder).

#include "../src/core/frame.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

namespace synth {

struct Style {
	uint8_t litR = 255, litG = 40, litB = 20; // leuchtendes Segment (Rot wie viele Hallentafeln)
	uint8_t bgR = 8, bgG = 8, bgB = 8;        // Hintergrund
	uint8_t ghost = 0;                        // Helligkeit abgeschalteter Segmente (Geisterbild)
	double strokeFrac = 0.13;                 // Strichstärke relativ zur Ziffernhöhe
	double widthFrac = 0.55;                  // Ziffernbreite relativ zur Höhe
	double slant = 0.0;                       // Kursivität (Verschiebung oben, relativ zur Breite)
	int bloom = 0;                            // Leuchtrand in Pixeln
	double noise = 0.0;                       // Rauschen (Standardabweichung 0..255)
	int blur = 0;                             // Anzahl 3x3-Weichzeichner-Durchläufe
};

struct Image {
	int w = 0, h = 0;
	std::vector<uint8_t> px; // BGRA

	Image(int w_, int h_, const Style &s) : w(w_), h(h_), px(size_t(w_) * size_t(h_) * 4)
	{
		for (size_t i = 0; i < px.size(); i += 4) {
			px[i] = s.bgB;
			px[i + 1] = s.bgG;
			px[i + 2] = s.bgR;
			px[i + 3] = 255;
		}
	}

	hb::FrameView view() const { return {px.data(), w, h, w * 4}; }

	void set(int x, int y, uint8_t r, uint8_t g, uint8_t b)
	{
		if (x < 0 || y < 0 || x >= w || y >= h)
			return;
		uint8_t *p = &px[(size_t(y) * size_t(w) + size_t(x)) * 4];
		p[0] = b;
		p[1] = g;
		p[2] = r;
		p[3] = 255;
	}
};

// Segmentbits wie im Leser: a=1 b=2 c=4 d=8 e=16 f=32 g=64
inline int maskFor(int digit)
{
	static const int m[10] = {63, 6, 91, 79, 102, 109, 125, 7, 127, 111};
	return m[digit];
}

inline void fillRect(Image &img, double x0, double y0, double x1, double y1, uint8_t r, uint8_t g, uint8_t b,
		     double slant, double top, double H, double W)
{
	for (int y = int(std::floor(y0)); y < int(std::ceil(y1)); ++y) {
		const double v = (y + 0.5 - top) / H;
		const double shift = slant * (0.5 - v) * W;
		for (int x = int(std::floor(x0 + shift)); x < int(std::ceil(x1 + shift)); ++x)
			img.set(x, y, r, g, b);
	}
}

// Zeichnet eine Ziffer (0-9) oder -1 für leer in die Box (x, y, Höhe H).
inline void drawDigit(Image &img, int digit, double x, double y, double H, const Style &s)
{
	const double W = H * s.widthFrac;
	const double t = H * s.strokeFrac;
	const int mask = digit >= 0 ? maskFor(digit) : 0;
	struct Seg {
		int bit;
		double x0, y0, x1, y1;
	};
	const double m = t * 0.55; // Aussparung an den Ecken
	const Seg segs[7] = {
		{1, x + m, y, x + W - m, y + t},
		{2, x + W - t, y + m, x + W, y + H / 2 - m * 0.3},
		{4, x + W - t, y + H / 2 + m * 0.3, x + W, y + H - m},
		{8, x + m, y + H - t, x + W - m, y + H},
		{16, x, y + H / 2 + m * 0.3, x + t, y + H - m},
		{32, x, y + m, x + t, y + H / 2 - m * 0.3},
		{64, x + m, y + H / 2 - t / 2, x + W - m, y + H / 2 + t / 2},
	};
	for (const Seg &sg : segs) {
		const bool on = (mask & sg.bit) != 0;
		if (!on && s.ghost == 0)
			continue;
		const uint8_t r = on ? s.litR : uint8_t(s.ghost);
		const uint8_t g = on ? s.litG : uint8_t(s.ghost / 3);
		const uint8_t b = on ? s.litB : uint8_t(s.ghost / 4);
		fillRect(img, sg.x0, sg.y0, sg.x1, sg.y1, r, g, b, s.slant, y, H, W);
	}
}

// Zeichnet eine Zahl rechtsbündig in `digits` Zellen; die Zellen beginnen bei x und sind je `pitch` breit.
inline void drawNumber(Image &img, int value, int digits, double x, double y, double H, double pitch,
		       const Style &s)
{
	const double W = H * s.widthFrac;
	std::vector<int> d(size_t(digits), -1);
	int v = value;
	for (int i = digits - 1; i >= 0; --i) {
		if (v == 0 && i < digits - 1 && value != 0 && false)
			break;
		d[size_t(i)] = v % 10;
		v /= 10;
		if (v == 0)
			break;
	}
	for (int i = 0; i < digits; ++i)
		drawDigit(img, d[size_t(i)], x + i * pitch + (pitch - W) * 0.5, y, H, s);
}

// Nachbearbeitung: Leuchtrand, Weichzeichnen, Rauschen.
inline void finish(Image &img, const Style &s, unsigned seed = 1)
{
	if (s.bloom > 0) {
		Image src = img;
		for (int y = 0; y < img.h; ++y)
			for (int x = 0; x < img.w; ++x) {
				uint8_t best[3] = {0, 0, 0};
				for (int dy = -s.bloom; dy <= s.bloom; ++dy)
					for (int dx = -s.bloom; dx <= s.bloom; ++dx) {
						int xx = std::clamp(x + dx, 0, img.w - 1);
						int yy = std::clamp(y + dy, 0, img.h - 1);
						const uint8_t *p = &src.px[(size_t(yy) * size_t(img.w) + size_t(xx)) * 4];
						for (int c = 0; c < 3; ++c)
							best[c] = std::max(best[c], p[c]);
					}
				uint8_t *q = &img.px[(size_t(y) * size_t(img.w) + size_t(x)) * 4];
				for (int c = 0; c < 3; ++c)
					q[c] = best[c];
			}
	}
	for (int pass = 0; pass < s.blur; ++pass) {
		Image src = img;
		for (int y = 0; y < img.h; ++y)
			for (int x = 0; x < img.w; ++x)
				for (int c = 0; c < 3; ++c) {
					int sum = 0, n = 0;
					for (int dy = -1; dy <= 1; ++dy)
						for (int dx = -1; dx <= 1; ++dx) {
							int xx = std::clamp(x + dx, 0, img.w - 1);
							int yy = std::clamp(y + dy, 0, img.h - 1);
							sum += src.px[(size_t(yy) * size_t(img.w) + size_t(xx)) * 4 + size_t(c)];
							++n;
						}
					img.px[(size_t(y) * size_t(img.w) + size_t(x)) * 4 + size_t(c)] = uint8_t(sum / n);
				}
	}
	if (s.noise > 0.0) {
		std::mt19937 rng(seed);
		std::normal_distribution<double> dist(0.0, s.noise);
		for (size_t i = 0; i < img.px.size(); i += 4)
			for (int c = 0; c < 3; ++c)
				img.px[i + size_t(c)] =
					uint8_t(std::clamp(int(std::lround(img.px[i + size_t(c)] + dist(rng))), 0, 255));
	}
}

// Normiertes Rechteck um `digits` Zellen mit etwas Rand (so, wie ein Nutzer es grob ziehen würde).
inline hb::NormRect rectAround(const Image &img, double x, double y, double H, double pitch, int digits,
			       double padPx)
{
	hb::NormRect r;
	r.x = (x - padPx) / img.w;
	r.y = (y - padPx) / img.h;
	r.w = (digits * pitch + 2 * padPx) / img.w;
	r.h = (H + 2 * padPx) / img.h;
	return r;
}

} // namespace synth
