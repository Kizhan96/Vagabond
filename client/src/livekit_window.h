#pragma once

#include <QMainWindow>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QLabel>
#include <QTabWidget>

class LiveKitWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit LiveKitWindow(QWidget *parent = nullptr);

private slots:
    void connectToLiveKit();
    void closeTab(int index);

private:
    void appendLog(const QString &line);

    QLineEdit *urlInput {nullptr};
    QLineEdit *tokenInput {nullptr};
    QLineEdit *roomInput {nullptr};
    QPushButton *connectButton {nullptr};
    QLabel *statusLabel {nullptr};
    QTextEdit *logView {nullptr};
    QTabWidget *tabWidget {nullptr};
};
