#pragma once

#include <QWidget>
#include <QWebEngineView>

class LiveKitRoomWidget : public QWidget {
    Q_OBJECT
public:
    explicit LiveKitRoomWidget(const QString &url, const QString &token, const QString &roomLabel, QWidget *parent = nullptr);

    QString title() const { return roomTitle; }

private:
    QString buildHtml(const QString &url, const QString &token, const QString &roomLabel) const;
    QString escapeForJs(const QString &value) const;

    QString roomTitle;
    QWebEngineView *webView {nullptr};
};
