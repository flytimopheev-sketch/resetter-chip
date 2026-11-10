#ifndef PROFILE_MANAGER_H
#define PROFILE_MANAGER_H

#include "CartridgeProfile.h"
#include "ChipProfile.h"

#include <QMap>
#include <QString>
#include <QVector>

#include <memory>

// Менеджер профилей чипов и картриджей. Загружает *.json из каталога профилей.
// Один файл может содержать как одиночный объект, так и массив профилей.
class ProfileManager
{
public:
    static ProfileManager &instance();

    bool loadProfiles(const QString &directoryPath);

    // Найти каталог профилей: настройки окружения, каталог установки, каталог сборки
    static QString defaultProfilesDirectory();

    // Загрузить из каталога по умолчанию
    bool loadDefaultProfiles();

    // Профиль чипа по id
    std::shared_ptr<ChipProfile> profile(const QString &id) const;

    // Профиль чипа или картриджа, приведённый к ChipProfile.
    // Для картриджа применяются переопределения счётчика и защищённых зон.
    std::shared_ptr<ChipProfile> resolveProfile(const QString &id) const;

    std::shared_ptr<CartridgeProfile> cartridgeProfile(const QString &id) const;

    QVector<QString> profileIds() const;
    QVector<QString> cartridgeProfileIds() const;
    QVector<std::shared_ptr<ChipProfile>> eepromProfiles() const;
    QVector<std::shared_ptr<CartridgeProfile>> cartridgeProfiles() const;

    int count() const { return m_profiles.size(); }
    int cartridgeCount() const { return m_cartridges.size(); }

    // Используется тестами и сменой каталога профилей
    void clear();

private:
    ProfileManager() = default;

    bool loadFile(const QString &filePath);
    bool addChipProfile(const ChipProfile &profile);
    bool addCartridgeProfile(const CartridgeProfile &profile);

    QMap<QString, std::shared_ptr<ChipProfile>> m_profiles;
    QMap<QString, std::shared_ptr<CartridgeProfile>> m_cartridges;
};

#endif // PROFILE_MANAGER_H
