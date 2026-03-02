#include <QApplication>
#include "ui/MainWindow.h"
#include "ui/SplashScreen.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("EleSim");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("EleSim");

    auto* splash = new SplashScreen();
    splash->show();
    app.processEvents();

    MainWindow window;
    splash->finish(&window);
    window.show();

    return app.exec();
}
