#include "ProfileManager.h"

#include "utils/Logger.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace {
constexpr quint32 kCounterUnset = 0xFFFFFFFFu;

bool looksLikeCartridgeProfile(const QJsonObject &obj)
{
    return obj.contains(QStringLiteral("chip_profile"));
}
} // namespace

ProfileManager &ProfileManager::instance()
{
    static ProfileManager inst;
    return inst;
}

void ProfileManager::clear()
{
    m_profiles.clear();
    m_cartridges.clear();
}

bool ProfileManager::addChipProfile(const ChipProfile &profile)
{
    if (!profile.isValid()) {
        Logger::instance().warning(QStringLiteral("Профиль чипа пропущен (некорректные поля): %1")
                                      .arg(profile.id.isEmpty() ? QStringLiteral("<без id>") : profile.id));
        return false;
    }

    if (m_profiles.contains(profile.id)) {
        Logger::instance().warning(QStringLiteral("Профиль чипа %1 переопределён").arg(profile.id));
    }

    m_profiles.insert(profile.id, std::make_shared<ChipProfile>(profile));
    Logger::instance().info(QStringLiteral("Загружен профиль чипа: %1 (%2)")
                                .arg(profile.id, profile.name));
    return true;
}

bool ProfileManager::addCartridgeProfile(const CartridgeProfile &profile)
{
    if (!profile.isValid()) {
        Logger::instance().warning(QStringLiteral("Профиль картриджа пропущен (некорректные поля): %1")
                                      .arg(profile.id.isEmpty() ? QStringLiteral("<без id>") : profile.id));
        return false;
    }

    if (m_cartridges.contains(profile.id)) {
        Logger::instance().warning(QStringLiteral("Профиль картриджа %1 переопределён").arg(profile.id));
    }

    m_cartridges.insert(profile.id, std::make_shared<CartridgeProfile>(profile));
    Logger::instance().info(QStringLiteral("Загружен профиль картриджа: %1 (%2)")
                                .arg(profile.id, profile.name));
    return true;
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

    const auto loadObject = [this, &loadedAny](const QJsonObject &obj) {
        if (looksLikeCartridgeProfile(obj)) {
            loadedAny = addCartridgeProfile(CartridgeProfile::fromJson(obj)) || loadedAny;
        } else {
            loadedAny = addChipProfile(ChipProfile::fromJson(obj)) || loadedAny;
        }
    };

    if (doc.isArray()) {
        const QJsonArray items = doc.array();
        for (const QJsonValue &value : items) {
            if (value.isObject()) {
                loadObject(value.toObject());
            }
        }
    } else if (doc.isObject()) {
        loadObject(doc.object());
    }

    if (!loadedAny) {
        Logger::instance().info(QStringLiteral("Файл не содержит корректных профилей (пропущен): ") + filePath);
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

    bool loadedAny = false;
    const QFileInfoList files = dir.entryInfoList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    for (const QFileInfo &info : files) {
        loadedAny = loadFile(info.absoluteFilePath()) || loadedAny;
    }

    return loadedAny;
}

QString ProfileManager::defaultProfilesDirectory()
{
    // Основное имя переменной — в верхнем регистре; старое имя оставлено
    // для совместимости с версией 1.0.0.
    QByteArray envPath = qgetenv("RESETTER_CHIP_PROFILES");
    if (envPath.isEmpty()) {
        envPath = qgetenv("Resetter_CHIP_PROFILES");
    }
    if (!envPath.isEmpty()) {
        const QString path = QString::fromLocal8Bit(envPath);
        if (QDir(path).exists()) {
            return path;
        }
        Logger::instance().warning(QStringLiteral("Каталог из RESETTER_CHIP_PROFILES не найден: ") + path);
    }

    QStringList candidates;

    // Каталог профилей пользователя (можно добавлять свои картриджи)
    const QString userData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!userData.isEmpty()) {
        candidates << userData + QStringLiteral("/profiles");
    }

    // Каталог рядом с исполняемым файлом (для сборки и для установки)
    const QString appDir = QCoreApplication::applicationDirPath();
    candidates << appDir + QStringLiteral("/profiles")
               << appDir + QStringLiteral("/../share/resetter-chip/profiles")
               << appDir + QStringLiteral("/../../share/resetter-chip/profiles")
               << appDir + QStringLiteral("/../src/profiles")
               << appDir + QStringLiteral("/../../src/profiles");

    // Системные пути установки (сначала актуальное имя, затем старое)
#ifdef Q_OS_UNIX
    candidates << QStringLiteral("/usr/share/resetter-chip/profiles")
               << QStringLiteral("/usr/local/share/resetter-chip/profiles")
               << QStringLiteral("/usr/share/Resetter-chip/profiles")
               << QStringLiteral("/usr/local/share/Resetter-chip/profiles");
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
    return m_profiles.value(id, nullptr);
}

std::shared_ptr<ChipProfile> ProfileManager::resolveProfile(const QString &id) const
{
    const auto chip = m_profiles.value(id, nullptr);
    if (chip) {
        return chip;
    }

    const auto cartridge = m_cartridges.value(id, nullptr);
    if (!cartridge) {
        return nullptr;
    }

    const auto base = m_profiles.value(cartridge->chipProfileId, nullptr);
    if (!base) {
        Logger::instance().error(QStringLiteral("Для картриджа %1 не найден профиль чипа %2")
                                     .arg(cartridge->id, cartridge->chipProfileId));
        return nullptr;
    }

    auto resolved = std::make_shared<ChipProfile>(*base);

    if (cartridge->counterOffset != kCounterUnset) {
        resolved->counterOffset = cartridge->counterOffset;
    }
    if (cartridge->counterSize != 0) {
        resolved->counterSize = cartridge->counterSize;
    }

    if (!cartridge->protectedRanges.isEmpty()) {
        resolved->protectedRanges.clear();
        for (const CartridgeRange &range : cartridge->protectedRanges) {
            resolved->protectedRanges.append(qMakePair(range.start, range.end));
        }
    }

    if (!cartridge->allowedModifyRanges.isEmpty()) {
        resolved->allowedRanges.clear();
        for (const CartridgeRange &range : cartridge->allowedModifyRanges) {
            resolved->allowedRanges.append(qMakePair(range.start, range.end));
        }
    }

    if (!resolved->isValid()) {
        Logger::instance().error(QStringLiteral("Профиль картриджа %1 содержит некорректные переопределения")
                                     .arg(cartridge->id));
        return nullptr;
    }

    return resolved;
}

std::shared_ptr<CartridgeProfile> ProfileManager::cartridgeProfile(const QString &id) const
{
    return m_cartridges.value(id, nullptr);
}

QVector<QString> ProfileManager::profileIds() const
{
    return m_profiles.keys();
}

QVector<QString> ProfileManager::cartridgeProfileIds() const
{
    return m_cartridges.keys();
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

QVector<std::shared_ptr<CartridgeProfile>> ProfileManager::cartridgeProfiles() const
{
    QVector<std::shared_ptr<CartridgeProfile>> result;
    for (auto it = m_cartridges.constBegin(); it != m_cartridges.constEnd(); ++it) {
        result.append(it.value());
    }
    return result;
}
