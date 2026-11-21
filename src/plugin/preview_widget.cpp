#include "preview_widget.hpp"

#include <QMouseEvent>
#include <QPainter>

#include <algorithm>

namespace hbp {

namespace {

QColor fieldColor(int field)
{
	static const QColor colors[FieldCount] = {
		QColor(255, 82, 82), QColor(64, 156, 255), QColor(76, 217, 100), QColor(255, 204, 0), QColor(220, 90, 255),
	};
	return colors[std::clamp(field, 0, int(FieldCount) - 1)];
}

} // namespace

PreviewWidget::PreviewWidget(QWidget *parent) : QWidget(parent)
{
	setMouseTracking(false);
	setCursor(Qt::CrossCursor);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	message_ = QString::fromUtf8("Keine Kamera gewählt");
}

void PreviewWidget::setImage(const QImage &image)
{
	image_ = image;
	update();
}

void PreviewWidget::clearImage()
{
	image_ = QImage();
	update();
}

void PreviewWidget::setMessage(const QString &message)
{
	if (message == message_)
		return;
	message_ = message;
	update();
}

void PreviewWidget::setFieldRect(int field, const hb::NormRect &rect, const QString &readingText)
{
	if (field < 0 || field >= FieldCount)
		return;
	rects_[field] = rect;
	texts_[field] = readingText;
}

void PreviewWidget::setActiveField(int field)
{
	activeField_ = std::clamp(field, 0, int(FieldCount) - 1);
	update();
}

QRectF PreviewWidget::imageRect() const
{
	if (image_.isNull())
		return QRectF(rect());
	QSizeF s = image_.size();
	s.scale(QSizeF(size()), Qt::KeepAspectRatio);
	return QRectF((width() - s.width()) / 2.0, (height() - s.height()) / 2.0, s.width(), s.height());
}

QPointF PreviewWidget::toNormalized(const QPoint &p) const
{
	const QRectF r = imageRect();
	return QPointF(std::clamp((p.x() - r.left()) / r.width(), 0.0, 1.0),
		       std::clamp((p.y() - r.top()) / r.height(), 0.0, 1.0));
}

void PreviewWidget::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	p.fillRect(rect(), QColor(20, 20, 24));

	if (image_.isNull()) {
		p.setPen(QColor(170, 170, 180));
		p.drawText(rect(), Qt::AlignCenter | Qt::TextWordWrap, message_);
		return;
	}

	p.setRenderHint(QPainter::SmoothPixmapTransform);
	const QRectF ir = imageRect();
	p.drawImage(ir, image_);

	p.setRenderHint(QPainter::Antialiasing);
	QFont f = font();
	f.setBold(true);
	f.setPixelSize(std::max(11, int(ir.height() / 14)));
	p.setFont(f);

	for (int i = 0; i < FieldCount; ++i) {
		if (!rects_[i].valid())
			continue;
		const QRectF r(ir.left() + rects_[i].x * ir.width(), ir.top() + rects_[i].y * ir.height(),
			       rects_[i].w * ir.width(), rects_[i].h * ir.height());
		QColor c = fieldColor(i);
		p.setPen(QPen(c, i == activeField_ ? 3 : 2));
		p.setBrush(QColor(c.red(), c.green(), c.blue(), i == activeField_ ? 40 : 18));
		p.drawRect(r);

		const QString label = texts_[i].isEmpty() ? QString::fromUtf8(fieldLabel(i)) : texts_[i];
		const QFontMetrics fm(f);
		const QRectF tag(r.left(), r.top() - fm.height() - 2, fm.horizontalAdvance(label) + 10, fm.height() + 2);
		const QRectF tagRect = tag.top() < ir.top() ? QRectF(r.left(), r.bottom() + 1, tag.width(), tag.height()) : tag;
		p.setPen(Qt::NoPen);
		p.setBrush(QColor(c.red(), c.green(), c.blue(), 220));
		p.drawRoundedRect(tagRect, 3, 3);
		p.setPen(QColor(0, 0, 0));
		p.drawText(tagRect, Qt::AlignCenter, label);
	}

	if (dragging_) {
		const QRectF r = QRectF(QPointF(ir.left() + dragStart_.x() * ir.width(), ir.top() + dragStart_.y() * ir.height()),
					QPointF(ir.left() + dragEnd_.x() * ir.width(), ir.top() + dragEnd_.y() * ir.height()))
					 .normalized();
		p.setBrush(Qt::NoBrush);
		p.setPen(QPen(fieldColor(activeField_), 2, Qt::DashLine));
		p.drawRect(r);
	}
}

void PreviewWidget::mousePressEvent(QMouseEvent *e)
{
	if (image_.isNull() || e->button() != Qt::LeftButton)
		return;
	dragging_ = true;
	dragStart_ = dragEnd_ = toNormalized(e->pos());
	update();
}

void PreviewWidget::mouseMoveEvent(QMouseEvent *e)
{
	if (!dragging_)
		return;
	dragEnd_ = toNormalized(e->pos());
	update();
}

void PreviewWidget::mouseReleaseEvent(QMouseEvent *e)
{
	if (!dragging_ || e->button() != Qt::LeftButton)
		return;
	dragging_ = false;
	dragEnd_ = toNormalized(e->pos());
	const QRectF r = QRectF(dragStart_, dragEnd_).normalized();
	update();
	if (r.width() > 0.005 && r.height() > 0.005)
		emit rectDrawn(activeField_, r.x(), r.y(), r.width(), r.height());
}

} // namespace hbp
