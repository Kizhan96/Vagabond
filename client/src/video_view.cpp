#include "video_view.h"

#include <QPainter>

VideoView::VideoView(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);
    setAutoFillBackground(false);
}

void VideoView::setFrame(const QPixmap &pix) {
    frame = pix;
    update();
}

void VideoView::clear() {
    frame = QPixmap();
    update();
}

void VideoView::setPlaceholder(const QString &text) {
    placeholder = text;
    if (frame.isNull()) update();
}

void VideoView::paintEvent(QPaintEvent *) {
    QPainter p(this);
    if (!p.isActive()) return;
    p.fillRect(rect(), Qt::black);
    if (!frame.isNull()) {
        const QPixmap scaled = frame.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        const QPoint topLeft((width() - scaled.width()) / 2, (height() - scaled.height()) / 2);
        p.drawPixmap(topLeft, scaled);
    } else {
        p.setPen(Qt::gray);
        p.drawText(rect(), Qt::AlignCenter, placeholder);
    }
}
