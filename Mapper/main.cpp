#include "MarioEmulatorUI.h"
#include <QtWidgets/QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    MarioEmulatorUI* window = new MarioEmulatorUI();
    window->show();

    return app.exec();
}