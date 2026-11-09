#include <QApplication>
#include <QDir>
#include <QStandardPaths>

#include "core/ProfileManager.h"
#include "gui/MainWindow.h"
#include "utils/Logger.h"

// Версия определяется CMake (project VERSION); значение ниже — запасное для IDE
#ifndef RESSETTER_CHIP_VERSION
#define RESSETTER_CHIP_VERSION "1.0.0"
#endif

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Ressetter Chip"));
    app.setApplicationVersion(QStringLiteral(RESSETTER_CHIP_VERSION));
    app.setApplicationDisplayName(QStringLiteral("Ressetter Chip"));
    app.setOrganizationName(QStringLiteral("RessetterChip"));
    app.setOrganizationDomain(QStringLiteral("ressetter-chip.local"));

    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString logDir = dataDir + QStringLiteral("/logs");
    QDir().mkpath(logDir);

    Logger::instance().init(logDir + QStringLiteral("/ressetter-chip.log"));
    Logger::instance().info(QStringLiteral("Запуск Ressetter Chip v")
                            + QStringLiteral(RESSETTER_CHIP_VERSION));

    // Профили чипов нужны до создания окна: их список попадает в интерфейс
    if (!ProfileManager::instance().loadDefaultProfiles()) {
        Logger::instance().warning(QStringLiteral("Профили чипов не загружены"));
    }

    MainWindow window;
    window.show();

    return app.exec();
}
