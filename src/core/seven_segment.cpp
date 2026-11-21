#include "seven_segment.hpp"

#include <array>
#include <cmath>
#include <vector>

namespace hb {

namespace {

constexpr int kSegA = 1, kSegB = 2, kSegC = 4, kSegD = 8, kSegE = 16, kSegF = 32, kSegG = 64;

struct Pattern {
	int mask;
	char digit;
};

// Mehrere Muster pro Ziffer, weil Tafeln unterschiedliche Schriftbilder haben
// (6/7/9 gibt es mit und ohne zusätzliches Segment).
constexpr std::array<Pattern, 14> kPatterns = {{
	{kSegA | kSegB | kSegC | kSegD | kSegE | kSegF, '0'},
	{kSegB | kSegC, '1'},
	{kSegA | kSegB | kSegD | kSegE | kSegG, '2'},
	{kSegA | kSegB | kSegC | kSegD | kSegG, '3'},
	{kSegB | kSegC | kSegF | kSegG, '4'},
	{kSegA | kSegC | kSegD | kSegF | kSegG, '5'},
	{kSegA | kSegC | kSegD | kSegE | kSegF | kSegG, '6'},
	{kSegC | kSegD | kSegE | kSegF | kSegG, '6'},
	{kSegA | kSegB | kSegC, '7'},
	{kSegA | kSegB | kSegC | kSegF, '7'},
	{kSegA | kSegB | kSegC | kSegD | kSegE | kSegF | kSegG, '8'},
	{kSegA | kSegB | kSegC | kSegD | kSegF | kSegG, '9'},
	{kSegA | kSegB | kSegC | kSegF | kSegG, '9'},
	{0, ' '},
}};

// Segmentzonen in Ziffern-Koordinaten (u: links->rechts, v: oben->unten, beide 0..1).
struct Zone {
	double u0, u1, v0, v1;
	int bit;
};

// Waagerechte Segmente (a, d, g) werden nur in der Mitte geprüft und senkrechte (b, c, e, f) nur
// abseits der Ecken. So lassen sich auch bei dicken Strichen z.B. "4" und "9" unterscheiden.
constexpr std::array<Zone, 7> kZones = {{
	{0.35, 0.65, 0.00, 0.14, kSegA},
	{0.72, 1.00, 0.18, 0.42, kSegB},
	{0.72, 1.00, 0.58, 0.82, kSegC},
	{0.35, 0.65, 0.86, 1.00, kSegD},
	{0.00, 0.28, 0.58, 0.82, kSegE},
	{0.00, 0.28, 0.18, 0.42, kSegF},
	{0.35, 0.65, 0.43, 0.57, kSegG},
}};

// Otsu-Schwelle auf einem 256er-Histogramm. Liefert Schwelle (Pixel > t gehören zur hellen Klasse)
// sowie die Mittelwerte beider Klassen.
struct Box {
	int l, r;
};

struct OtsuResult {
	int threshold = 128;
	double meanDark = 0.0;
	double meanBright = 0.0;
	long countDark = 0;
	long countBright = 0;
};

OtsuResult otsu(const std::array<long, 256> &hist, long total)
{
	double sumAll = 0;
	for (int i = 0; i < 256; ++i)
		sumAll += double(i) * double(hist[i]);

	double sumB = 0;
	long wB = 0;
	double bestVar = -1.0;
	OtsuResult res;

	for (int t = 0; t < 256; ++t) {
		wB += hist[t];
		if (wB == 0)
			continue;
		long wF = total - wB;
		if (wF == 0)
			break;
		sumB += double(t) * double(hist[t]);
		double mB = sumB / double(wB);
		double mF = (sumAll - sumB) / double(wF);
		double var = double(wB) * double(wF) * (mB - mF) * (mB - mF);
		if (var > bestVar) {
			bestVar = var;
			res.threshold = t;
			res.meanDark = mB;
			res.meanBright = mF;
			res.countDark = wB;
			res.countBright = wF;
		}
	}
	return res;
}

} // namespace

FieldReading SevenSegmentReader::read(const FrameView &frame, const NormRect &rectIn, int digits,
				      const SegmentOptions &opt)
{
	FieldReading out;
	if (!frame.valid() || !rectIn.valid() || digits < 1 || digits > 6)
		return out;

	const NormRect rect = rectIn.clamped();
	const int x0 = std::clamp(int(std::floor(rect.x * frame.width)), 0, frame.width - 1);
	const int y0 = std::clamp(int(std::floor(rect.y * frame.height)), 0, frame.height - 1);
	const int x1 = std::clamp(int(std::ceil((rect.x + rect.w) * frame.width)), x0 + 1, frame.width);
	const int y1 = std::clamp(int(std::ceil((rect.y + rect.h) * frame.height)), y0 + 1, frame.height);
	const int w = x1 - x0;
	const int h = y1 - y0;
	if (w < digits * 4 || h < 8)
		return out; // Bereich ist zu klein, um Segmente aufzulösen

	// 1) Helligkeitsebene: hellster Kanal je Pixel.
	std::vector<uint8_t> v(size_t(w) * size_t(h));
	std::array<long, 256> hist{};
	for (int y = 0; y < h; ++y) {
		const uint8_t *row = frame.data + size_t(y0 + y) * size_t(frame.stride) + size_t(x0) * 4;
		uint8_t *dst = &v[size_t(y) * size_t(w)];
		for (int x = 0; x < w; ++x) {
			const uint8_t *p = row + x * 4;
			const uint8_t m = std::max({p[0], p[1], p[2]});
			dst[x] = m;
			++hist[m];
		}
	}

	// 2) Hell/dunkel trennen.
	const long total = long(w) * long(h);
	const OtsuResult o = otsu(hist, total);
	if (o.countDark == 0 || o.countBright == 0 || (o.meanBright - o.meanDark) < opt.minContrast)
		return out; // Status Blank

	bool litIsBright;
	switch (opt.polarity) {
	case Polarity::BrightOnDark:
		litIsBright = true;
		break;
	case Polarity::DarkOnBright:
		litIsBright = false;
		break;
	default:
		litIsBright = o.countBright <= o.countDark; // leuchtende Segmente sind die Minderheit
		break;
	}

	std::vector<uint8_t> lit(v.size());
	long litCount = 0;
	for (size_t i = 0; i < v.size(); ++i) {
		const bool isBright = v[i] > o.threshold;
		const bool l = (isBright == litIsBright);
		lit[i] = l ? 1 : 0;
		litCount += l;
	}
	if (double(litCount) < 0.01 * double(total))
		return out; // praktisch nichts leuchtet

	// 3) Vertikal auf leuchtende Zeilen zuschneiden.
	const int rowMin = std::max(1, int(std::lround(0.03 * w)));
	int top = -1, bottom = -1;
	for (int y = 0; y < h; ++y) {
		int c = 0;
		const uint8_t *row = &lit[size_t(y) * size_t(w)];
		for (int x = 0; x < w; ++x)
			c += row[x];
		if (c >= rowMin) {
			if (top < 0)
				top = y;
			bottom = y;
		}
	}
	if (top < 0 || (bottom - top + 1) < std::max(8, int(0.25 * h))) {
		out.status = ReadStatus::Unreadable;
		return out;
	}
	const int digitH = bottom - top + 1;

	// 4) Kursive Ziffern aufrichten: jede Zeile um ihren Versatz zurückschieben.
	const int pad = int(std::ceil(std::fabs(opt.slant) * 0.6 * digitH)) + 1;
	const int w2 = w + 2 * pad;
	std::vector<uint8_t> up(size_t(w2) * size_t(digitH), 0);
	for (int y = 0; y < digitH; ++y) {
		const double vv = (double(y) + 0.5) / double(digitH);
		const int shift = int(std::lround(opt.slant * (0.5 - vv) * 0.55 * digitH));
		const uint8_t *src = &lit[size_t(top + y) * size_t(w)];
		uint8_t *dst = &up[size_t(y) * size_t(w2)];
		for (int x = 0; x < w; ++x) {
			const int nx = x + pad - shift;
			if (nx >= 0 && nx < w2)
				dst[nx] = src[x];
		}
	}

	// 5) Spaltenprofil: zusammenhängende Spalten mit leuchtenden Pixeln = eine Ziffer.
	//    (Jede 7-Segment-Ziffer hängt zusammen, weil sie waagerechte Balken hat; nur die "1" ist
	//     ein einzelner senkrechter Balken.)
	const int colMin = std::max(1, int(std::lround(0.04 * digitH)));
	struct Run {
		int l, r;
	};
	std::vector<Run> runs;
	{
		int start = -1;
		for (int x = 0; x <= w2; ++x) {
			bool occupied = false;
			if (x < w2) {
				int c = 0;
				for (int y = 0; y < digitH; ++y)
					c += up[size_t(y) * size_t(w2) + size_t(x)];
				occupied = c >= colMin;
			}
			if (occupied && start < 0)
				start = x;
			if (!occupied && start >= 0) {
				if (x - start >= std::max(2, int(0.06 * digitH))) {
					// Ziffern reichen fast über die ganze Höhe. Doppelpunkt, Dezimalpunkt und
					// Störpixel sind deutlich kürzer und werden ignoriert.
					int first = -1, last = -1;
					for (int y = 0; y < digitH; ++y) {
						for (int cx = start; cx < x; ++cx) {
							if (up[size_t(y) * size_t(w2) + size_t(cx)]) {
								if (first < 0)
									first = y;
								last = y;
								break;
							}
						}
					}
					if (first >= 0 && (last - first + 1) >= int(0.6 * digitH))
						runs.push_back({start, x - 1});
				}
				start = -1;
			}
		}
	}
	if (runs.empty()) {
		out.status = ReadStatus::Unreadable;
		return out;
	}

	// Zu breite Läufe sind zusammengelaufene Ziffern (Unschärfe/Leuchtrand): gleichmäßig teilen.
	std::vector<Box> boxes;
	for (const Run &r : runs) {
		const double rw = double(r.r - r.l + 1);
		const double pitch = 0.66 * digitH;
		int m = std::clamp(int(std::lround((rw + 0.1 * digitH) / pitch)), 1, digits);
		if (rw < 0.8 * digitH)
			m = 1;
		for (int i = 0; i < m; ++i) {
			const int a = r.l + int(std::lround(rw * i / m));
			const int b = r.l + int(std::lround(rw * (i + 1) / m)) - 1;
			boxes.push_back({a, std::max(a, b)});
		}
	}
	if (int(boxes.size()) > digits) {
		out.status = ReadStatus::Unreadable;
		return out;
	}
	// Eine fehlende Ziffer mitten in der Zahl ("1 2") ergibt eine ungewöhnlich große Lücke
	// (eine "1" sitzt rechtsbündig in ihrer Zelle, daher ist auch ein normaler Abstand schon groß).
	for (size_t i = 1; i < boxes.size(); ++i) {
		if (boxes[i].l - boxes[i - 1].r > 0.8 * digitH) {
			out.status = ReadStatus::Unreadable;
			return out;
		}
	}

	// 6) Jede Ziffer auswerten.
	out.status = ReadStatus::Ok;
	out.text.assign(size_t(digits - int(boxes.size())), ' ');
	double conf = 1.0;
	long result = 0;

	for (const Box &box : boxes) {
		const int bw = box.r - box.l + 1;
		char ch = '?';
		double digitConf = 1.0;

		if (bw < 0.34 * digitH) {
			// Schmaler, hoher Balken -> "1" (rechte Segmente b und c)
			int rowsLit = 0;
			const int ya = int(0.08 * digitH), yb = int(0.92 * digitH);
			for (int y = ya; y <= yb; ++y) {
				bool any = false;
				for (int x = box.l; x <= box.r && !any; ++x)
					any = up[size_t(y) * size_t(w2) + size_t(x)] != 0;
				rowsLit += any;
			}
			const double cover = double(rowsLit) / double(std::max(1, yb - ya + 1));
			if (cover >= 0.8) {
				ch = '1';
				digitConf = std::min(1.0, (cover - 0.5) / 0.5);
			} else {
				digitConf = 0.0;
			}
		} else {
			int mask = 0;
			for (const Zone &z : kZones) {
				long on = 0, n = 0;
				const int ya = int(std::floor(z.v0 * digitH));
				const int yb = std::max(ya, int(std::ceil(z.v1 * digitH)) - 1);
				const int xa = box.l + int(std::lround(z.u0 * bw));
				const int xb = std::max(xa, box.l + int(std::lround(z.u1 * bw)) - 1);
				for (int y = ya; y <= std::min(yb, digitH - 1); ++y) {
					const uint8_t *row = &up[size_t(y) * size_t(w2)];
					for (int x = xa; x <= std::min(xb, w2 - 1); ++x) {
						on += row[x];
						++n;
					}
				}
				const double frac = n > 0 ? double(on) / double(n) : 0.0;
				const double thr = opt.segmentThreshold;
				double score;
				if (frac >= thr) {
					mask |= z.bit;
					score = (frac - thr) / (1.0 - thr);
				} else {
					score = (thr - frac) / thr;
				}
				digitConf = std::min(digitConf, score);
			}
			for (const Pattern &p : kPatterns) {
				if (p.mask == mask && p.digit != ' ') {
					ch = p.digit;
					break;
				}
			}
		}

		out.text.push_back(ch);
		if (ch == '?') {
			out.status = ReadStatus::Unreadable;
			continue;
		}
		result = result * 10 + (ch - '0');
		conf = std::min(conf, digitConf);
	}

	if (out.status != ReadStatus::Ok) {
		out.confidence = 0.0;
		return out;
	}
	out.value = int(result);
	out.confidence = std::clamp(conf, 0.0, 1.0);
	return out;
}

} // namespace hb
