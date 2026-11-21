#pragma once
// Kamerabild mit den eingezeichneten Feldern. Im Bild ziehen = Bereich für das gewählte Feld festlegen.

#include "config.hpp"

#include <QImage>
#include <QPoint>
#include <QString>
#include <QWidget>

namespace hbp {

class PreviewWidget : public QWidget {
	Q_OBJECT
public:
	explicit PreviewWidget(QWidget *parent = nullptr);

	void setImage(const QImage &image);
	void clearImage();
	void setFieldRect(int field, const hb::NormRect &rect, const QString &readingText);
	void setActiveField(int field);
	void setMessage(const QString &message);

	QSize minimumSizeHint() const override { return QSize(240, 150); }
	QSize sizeHint() const override { return QSize(480, 270); }

signals:
	void rectDrawn(int field, double x, double y, double w, double h);

protected:
	void paintEvent(QPaintEvent *) override;
	void mousePressEvent(QMouseEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;

private:
	QRectF imageRect() const;
	QPointF toNormalized(const QPoint &p) const;

	QImage image_;
	QString message_;
	hb::NormRect rects_[FieldCount];
	QString texts_[FieldCount];
	int activeField_ = 0;
	bool dragging_ = false;
	QPointF dragStart_, dragEnd_;
};

} // namespace hbp
