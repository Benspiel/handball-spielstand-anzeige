// Erzeugt ein künstliches Kamerabild einer Hallen-Anzeigetafel (PPM) für Demo und Tests.
// Aufruf: make_demo_board <heim> <gast> <min> <sek> <halbzeit> <ausgabe.ppm>
// Gibt die Rahmen (normiert) für die einzelnen Felder als JSON auf stdout aus.

#include "synthetic_board.hpp"

#include <cstdio>
#include <cstdlib>

int main(int argc, char **argv)
{
	if (argc < 7) {
		std::fprintf(stderr, "Aufruf: %s heim gast min sek halbzeit ausgabe.ppm\n", argv[0]);
		return 1;
	}
	const int home = std::atoi(argv[1]), away = std::atoi(argv[2]), mm = std::atoi(argv[3]),
		  ss = std::atoi(argv[4]), half = std::atoi(argv[5]);

	synth::Style st;
	st.noise = 6;
	st.blur = 1;
	st.bloom = 1;
	st.bgR = st.bgG = st.bgB = 6;
	st.ghost = 28; // abgeschaltete Segmente schimmern leicht

	synth::Image img(1280, 720, st);
	// Hallenwand außen herum
	for (int y = 0; y < img.h; ++y)
		for (int x = 0; x < img.w; ++x)
			if (x < 190 || x > 1090 || y < 150 || y > 570)
				img.set(x, y, uint8_t(58 + y / 24), uint8_t(62 + y / 24), uint8_t(70 + y / 24));

	const double sH = 150, sPitch = sH * st.widthFrac * 1.35;
	const double cH = 90, cPitch = cH * st.widthFrac * 1.35;
	const double hH = 70, hPitch = hH * st.widthFrac * 1.35;

	const double homeX = 235, awayX = 820, scoreY = 330;
	const double minX = 500, secX = 662, clockY = 205;
	const double halfX = 610, halfY = 450;

	synth::drawNumber(img, home, 2, homeX, scoreY, sH, sPitch, st);
	synth::drawNumber(img, away, 2, awayX, scoreY, sH, sPitch, st);
	synth::drawNumber(img, mm, 2, minX, clockY, cH, cPitch, st);
	synth::drawNumber(img, ss, 2, secX, clockY, cH, cPitch, st);
	synth::drawNumber(img, half, 1, halfX, halfY, hH, hPitch, st);
	// Doppelpunkt
	for (double cy : {clockY + cH * 0.3, clockY + cH * 0.7})
		for (int dy = -4; dy <= 4; ++dy)
			for (int dx = -4; dx <= 4; ++dx)
				img.set(int(secX - 17 + dx), int(cy + dy), st.litR, st.litG, st.litB);
	synth::finish(img, st, 7);

	std::FILE *f = std::fopen(argv[6], "wb");
	if (!f)
		return 2;
	std::fprintf(f, "P6\n%d %d\n255\n", img.w, img.h);
	for (int i = 0; i < img.w * img.h; ++i) {
		const uint8_t *p = &img.px[size_t(i) * 4];
		const uint8_t rgb[3] = {p[2], p[1], p[0]};
		std::fwrite(rgb, 1, 3, f);
	}
	std::fclose(f);

	auto emit = [&](const char *key, double x, double y, double H, double pitch, int digits, const char *tail) {
		const hb::NormRect r = synth::rectAround(img, x, y, H, pitch, digits, 14);
		std::printf("    \"%s\": {\"x\": %.5f, \"y\": %.5f, \"w\": %.5f, \"h\": %.5f, \"digits\": %d}%s\n", key, r.x,
			    r.y, r.w, r.h, digits, tail);
	};
	std::printf("{\n");
	emit("home", homeX, scoreY, sH, sPitch, 2, ",");
	emit("away", awayX, scoreY, sH, sPitch, 2, ",");
	emit("minutes", minX, clockY, cH, cPitch, 2, ",");
	emit("seconds", secX, clockY, cH, cPitch, 2, ",");
	emit("half", halfX, halfY, hH, hPitch, 1, "");
	std::printf("}\n");
	return 0;
}
