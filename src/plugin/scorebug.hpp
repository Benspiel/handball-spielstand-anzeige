#pragma once
// Zeichnet die Spielstand-Grafik (Scorebug) in ein QImage.

#include "shared_state.hpp"

#include <QImage>
#include <QString>

namespace hbp {

constexpr int kScorebugWidth = 1400;
constexpr int kScorebugHeight = 200;

struct ScorebugStyle {
	QString fontFamily = "Segoe UI";
	int theme = 0; // 0 = dunkel, 1 = hell
	bool showTime = true;
	bool showHalf = true;
};

QString formatClock(int seconds);
QString halfLabel(int half);

// Liefert ein vormultipliziertes RGBA-Bild der Größe kScorebugWidth x kScorebugHeight.
QImage renderScorebug(const Snapshot &snapshot, const ScorebugStyle &style);

} // namespace hbp
