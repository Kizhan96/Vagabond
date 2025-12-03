#ifndef VIDEO_VIEW_H
#define VIDEO_VIEW_H

#include <QWidget>
#include <QPixmap>

class VideoView : public QWidget {
    Q_OBJECT
public:
    explicit VideoView(QWidget *parent = nullptr);
    void setFrame(const QPixmap &pix);
    void clear();
    void setPlaceholder(const QString &text);
    QPixmap currentFrame() const { return frame; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QPixmap frame;
    QString placeholder = QStringLiteral("Screen preview");
};

#endif // VIDEO_VIEW_H
