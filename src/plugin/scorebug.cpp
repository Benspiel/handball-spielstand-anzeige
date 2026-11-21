#include "scorebug.hpp"

#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>

namespace hbp {

QString formatClock(int seconds)
{
	seconds = std::max(0, seconds);
	return QString("%1:%2").arg(seconds / 60, 2, 10, QChar('0')).arg(seconds % 60, 2, 10, QChar('0'));
}

QString halfLabel(int half)
{
	switch (half) {
	case 1:
		return QString::fromUtf8("1. HALBZEIT");
	case 2:
		return QString::fromUtf8("2. HALBZEIT");
	case 3:
		return QString::fromUtf8("VERLÄNGERUNG 1");
	case 4:
		return QString::fromUtf8("VERLÄNGERUNG 2");
	default:
		return QString();
	}
}

namespace {

QColor fromArgb(uint32_t argb)
{
	return QColor::fromRgba(argb);
}

bool isBright(const QColor &c)
{
	return (0.299 * c.red() + 0.587 * c.green() + 0.114 * c.blue()) > 170.0;
}

// Schrift so verkleinern, dass der Text in die Breite passt.
QFont fitFont(const QString &family, QFont::Weight weight, const QString &text, double maxPx, double maxW,
	      double minPx = 14.0)
{
	QFont f(family);
	f.setWeight(weight);
	double px = maxPx;
	for (;;) {
		f.setPixelSize(int(px));
		if (QFontMetrics(f).horizontalAdvance(text) <= maxW || px <= minPx)
			break;
		px -= 2.0;
	}
	return f;
}

// Wie fitFont, erlaubt aber Zeilenumbruch: größte Schrift, bei der der Text in das Rechteck passt.
QFont fitFontWrapped(const QString &family, QFont::Weight weight, const QString &text, double maxPx,
		     const QSizeF &box, int alignFlags, double minPx = 22.0)
{
	QFont f(family);
	f.setWeight(weight);
	double px = maxPx;
	for (;;) {
		f.setPixelSize(int(px));
		const QRectF need = QFontMetricsF(f).boundingRect(QRectF(0, 0, box.width(), 10000.0),
								   alignFlags | Qt::TextWordWrap, text);
		if ((need.height() <= box.height() && need.width() <= box.width() + 0.5) || px <= minPx)
			break;
		px -= 2.0;
	}
	return f;
}

void drawCentered(QPainter &p, const QRectF &r, const QString &text, const QFont &font, const QColor &color)
{
	p.setFont(font);
	p.setPen(color);
	p.drawText(r, Qt::AlignCenter, text);
}

// Ziffern in gleich breiten Zellen zeichnen, damit nichts wackelt, wenn sich eine Ziffer ändert.
void drawTabular(QPainter &p, const QRectF &r, const QString &text, const QFont &font, const QColor &color)
{
	p.setFont(font);
	p.setPen(color);
	const QFontMetrics fm(font);
	double cell = 0;
	for (QChar c : QString("0123456789"))
		cell = std::max(cell, double(fm.horizontalAdvance(c)));
	const double colonW = fm.horizontalAdvance(QChar(':'));

	double total = 0;
	for (QChar c : text)
		total += c.isDigit() ? cell : colonW;
	double x = r.center().x() - total / 2.0;
	for (QChar c : text) {
		const double w = c.isDigit() ? cell : colonW;
		p.drawText(QRectF(x, r.top(), w, r.height()), Qt::AlignCenter, QString(c));
		x += w;
	}
}

} // namespace

QImage renderScorebug(const Snapshot &snap, const ScorebugStyle &style)
{
	QImage img(kScorebugWidth, kScorebugHeight, QImage::Format_RGBA8888_Premultiplied);
	img.fill(Qt::transparent);

	QPainter p(&img);
	p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);

	const bool dark = style.theme == 0;
	const QColor barTop = dark ? QColor(30, 36, 50) : QColor(250, 250, 252);
	const QColor barBottom = dark ? QColor(14, 17, 26) : QColor(226, 230, 238);
	const QColor textMain = dark ? QColor(255, 255, 255) : QColor(18, 22, 32);
	const QColor centerBg = dark ? QColor(8, 10, 16) : QColor(32, 38, 54);
	const QColor centerText = QColor(255, 255, 255);

	const QRectF bar(0, 30, kScorebugWidth, 140);
	const QRectF center(535, 8, 330, 184);

	// Balken mit Farbverlauf und Schatten
	QPainterPath barPath;
	barPath.addRoundedRect(bar, 28, 28);
	{
		QLinearGradient g(bar.topLeft(), bar.bottomLeft());
		g.setColorAt(0, barTop);
		g.setColorAt(1, barBottom);
		p.fillPath(barPath, g);
	}

	// Teamfarben als Streifen an den Außenkanten (am Balken abgeschnitten)
	p.save();
	p.setClipPath(barPath);
	p.fillRect(QRectF(0, bar.top(), 18, bar.height()), fromArgb(snap.homeColor));
	p.fillRect(QRectF(kScorebugWidth - 18, bar.top(), 18, bar.height()), fromArgb(snap.awayColor));
	p.restore();

	// Teamnamen
	const QString homeName = QString::fromStdString(snap.homeName).toUpper();
	const QString awayName = QString::fromStdString(snap.awayName).toUpper();
	{
		const QRectF homeRect(44, bar.top(), 355, bar.height());
		const QRectF awayRect(kScorebugWidth - 44 - 355, bar.top(), 355, bar.height());
		const int homeFlags = Qt::AlignVCenter | Qt::AlignLeft;
		const int awayFlags = Qt::AlignVCenter | Qt::AlignRight;
		const QSizeF nameBox(homeRect.width(), homeRect.height() - 20);
		// Erst einzeilig versuchen (bis 40 px runter), erst danach auf zwei Zeilen umbrechen.
		auto fitName = [&](const QString &text, int flags) {
			QFont f = fitFont(style.fontFamily, QFont::Bold, text, 58, nameBox.width(), 40);
			if (QFontMetrics(f).horizontalAdvance(text) <= nameBox.width())
				return f;
			return fitFontWrapped(style.fontFamily, QFont::Bold, text, 52, nameBox, flags);
		};
		p.setFont(fitName(homeName, homeFlags));
		p.setPen(textMain);
		p.drawText(homeRect, homeFlags | Qt::TextWordWrap, homeName);
		p.setFont(fitName(awayName, awayFlags));
		p.setPen(textMain);
		p.drawText(awayRect, awayFlags | Qt::TextWordWrap, awayName);
	}

	// Torfelder in Teamfarbe
	auto scoreBox = [&](const QRectF &r, uint32_t argb, int goals) {
		const QColor c = fromArgb(argb);
		QPainterPath path;
		path.addRoundedRect(r, 20, 20);
		QLinearGradient g(r.topLeft(), r.bottomLeft());
		g.setColorAt(0, c.lighter(115));
		g.setColorAt(1, c.darker(125));
		p.fillPath(path, g);
		const QString t = QString::number(goals);
		const QFont f = fitFont(style.fontFamily, QFont::Black, t, 100, r.width() - 16, 40);
		drawCentered(p, r, t, f, isBright(c) ? QColor(15, 18, 26) : QColor(255, 255, 255));
	};
	scoreBox(QRectF(405, bar.top() + 14, 120, bar.height() - 28), snap.homeColor, snap.match.home);
	scoreBox(QRectF(kScorebugWidth - 405 - 120, bar.top() + 14, 120, bar.height() - 28), snap.awayColor,
		 snap.match.away);

	// Mittelblock: Zeit und Halbzeit
	if (style.showTime || style.showHalf) {
		QPainterPath cp;
		cp.addRoundedRect(center, 26, 26);
		p.fillPath(cp, centerBg);
		p.setPen(QPen(QColor(255, 255, 255, dark ? 40 : 60), 2));
		p.drawPath(cp);

		const bool both = style.showTime && style.showHalf;
		if (style.showTime) {
			const QRectF tr = both ? QRectF(center.left(), center.top() + 16, center.width(), 104)
					       : QRectF(center.left(), center.top(), center.width(), center.height());
			const QFont tf = [&] {
				QFont f(style.fontFamily);
				f.setWeight(QFont::Black);
				f.setPixelSize(both ? 92 : 100);
				return f;
			}();
			drawTabular(p, tr, formatClock(snap.match.clockSeconds), tf, centerText);
		}
		if (style.showHalf) {
			const QString hl = halfLabel(snap.match.half);
			const QRectF chip = both ? QRectF(center.left() + 36, center.bottom() - 62, center.width() - 72, 40)
						 : QRectF(center.left() + 36, center.top() + 70, center.width() - 72, 44);
			QPainterPath chipPath;
			chipPath.addRoundedRect(chip, chip.height() / 2, chip.height() / 2);
			p.fillPath(chipPath, QColor(255, 255, 255, 30));
			QFont lf = fitFont(style.fontFamily, QFont::Bold, hl, 26, chip.width() - 24, 14);
			lf.setLetterSpacing(QFont::AbsoluteSpacing, 2.0);
			drawCentered(p, chip, hl, lf, QColor(230, 235, 245));
		}
	}

	p.end();
	return img;
}

} // namespace hbp
