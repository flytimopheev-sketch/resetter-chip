#include <QApplication>
#include <QDir>
#include <QStandardPaths>

#include "core/ProfileManager.h"
#include "gui/MainWindow.h"
#include "utils/Logger.h"

// Версия определяется CMake (project VERSION); значение ниже — запасное для IDE
#ifndef RESETTER_CHIP_VERSION
#define RESETTER_CHIP_VERSION "1.1.0"
#endif

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Resetter Chip"));
    app.setApplicationVersion(QStringLiteral(RESETTER_CHIP_VERSION));
    app.setApplicationDisplayName(QStringLiteral("Resetter Chip"));
    app.setOrganizationName(QStringLiteral("ResetterChip"));
    app.setOrganizationDomain(QStringLiteral("Resetter-chip.local"));

    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString logDir = dataDir + QStringLiteral("/logs");
    QDir().mkpath(logDir);

    Logger::instance().init(logDir + QStringLiteral("/Resetter-chip.log"));
    Logger::instance().info(QStringLiteral("Запуск Resetter Chip v")
                            + QStringLiteral(RESETTER_CHIP_VERSION));

    // Профили чипов нужны до создания окна: их список попадает в интерфейс
    if (!ProfileManager::instance().loadDefaultProfiles()) {
        Logger::instance().warning(QStringLiteral("Профили чипов не загружены"));
    }

    MainWindow window;
    window.show();

    return app.exec();
}
