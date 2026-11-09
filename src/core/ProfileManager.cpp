#include "ProfileManager.h"

#include "utils/Logger.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

ProfileManager &ProfileManager::instance()
{
    static ProfileManager inst;
    return inst;
}

void ProfileManager::addProfile(const ChipProfile &profile)
{
    if (profile.id.isEmpty() || profile.sizeBytes == 0) {
        return;
    }
    m_profiles.insert(profile.id, std::make_shared<ChipProfile>(profile));
    Logger::instance().info(QStringLiteral("Загружен профиль чипа: %1 (%2)")
                                .arg(profile.id, profile.name));
}

bool ProfileManager::loadFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        Logger::instance().warning(QStringLiteral("Не удалось открыть профиль: ") + filePath);
        return false;
    }

    const QByteArray raw = file.readAll();
    file.close();

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        Logger::instance().warning(QStringLiteral("Ошибка JSON в %1: %2")
                                       .arg(QFileInfo(filePath).fileName(), parseError.errorString()));
        return false;
    }

    bool loadedAny = false;

    if (doc.isArray()) {
        const QJsonArray items = doc.array();
        for (const QJsonValue &value : items) {
            if (!value.isObject()) {
                continue;
            }
            const ChipProfile profile = ChipProfile::fromJson(value.toObject());
            if (!profile.id.isEmpty() && profile.sizeBytes > 0) {
                addProfile(profile);
                loadedAny = true;
            }
        }
    } else if (doc.isObject()) {
        const ChipProfile profile = ChipProfile::fromJson(doc.object());
        if (!profile.id.isEmpty() && profile.sizeBytes > 0) {
            addProfile(profile);
            loadedAny = true;
        }
    }

    if (!loadedAny) {
        Logger::instance().info(QStringLiteral("Файл не содержит профилей чипов (пропущен): ") + filePath);
    }

    return loadedAny;
}

bool ProfileManager::loadProfiles(const QString &directoryPath)
{
    QDir dir(directoryPath);
    if (!dir.exists()) {
        Logger::instance().warning(QStringLiteral("Каталог профилей не найден: ") + directoryPath);
        return false;
    }

    const QFileInfoList files = dir.entryInfoList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    for (const QFileInfo &info : files) {
        loadFile(info.absoluteFilePath());
    }

    return !m_profiles.isEmpty();
}

QString ProfileManager::defaultProfilesDirectory()
{
    // 1) Явное переопределение через переменную окружения
    const QByteArray envPath = qgetenv("RESSETTER_CHIP_PROFILES");
    if (!envPath.isEmpty()) {
        const QString path = QString::fromLocal8Bit(envPath);
        if (QDir(path).exists()) {
            return path;
        }
    }

    QStringList candidates;

    // 2) Каталог рядом с исполняемым файлом (для сборки и для установки)
    const QString appDir = QCoreApplication::applicationDirPath();
    candidates << appDir + QStringLiteral("/profiles")
               << appDir + QStringLiteral("/../share/ressetter-chip/profiles")
               << appDir + QStringLiteral("/../src/profiles")
               << appDir + QStringLiteral("/../../src/profiles");

    // 3) Системные пути установки
#ifdef Q_OS_UNIX
    candidates << QStringLiteral("/usr/share/ressetter-chip/profiles")
               << QStringLiteral("/usr/local/share/ressetter-chip/profiles");
#endif

    for (const QString &candidate : candidates) {
        const QDir dir(QDir::cleanPath(candidate));
        if (dir.exists()) {
            return dir.absolutePath();
        }
    }

    return QString();
}

bool ProfileManager::loadDefaultProfiles()
{
    const QString dir = defaultProfilesDirectory();
    if (dir.isEmpty()) {
        Logger::instance().warning(QStringLiteral("Каталог профилей не найден ни в одном из стандартных мест"));
        return false;
    }
    Logger::instance().info(QStringLiteral("Каталог профилей: ") + dir);
    return loadProfiles(dir);
}

std::shared_ptr<ChipProfile> ProfileManager::profile(const QString &id) const
{
    const auto it = m_profiles.constFind(id);
    if (it != m_profiles.constEnd()) {
        return it.value();
    }
    return nullptr;
}

QVector<QString> ProfileManager::profileIds() const
{
    return m_profiles.keys().toVector();
}

QVector<std::shared_ptr<ChipProfile>> ProfileManager::eepromProfiles() const
{
    QVector<std::shared_ptr<ChipProfile>> result;
    for (auto it = m_profiles.constBegin(); it != m_profiles.constEnd(); ++it) {
        if (it.value()->family.startsWith(QStringLiteral("24C"), Qt::CaseInsensitive)) {
            result.append(it.value());
        }
    }
    return result;
}

