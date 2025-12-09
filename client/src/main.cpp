#include <QApplication>
#include "livekit_window.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    LiveKitWindow window;
    window.show();
    return app.exec();
}
