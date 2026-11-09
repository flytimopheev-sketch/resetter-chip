#ifndef PROFILE_MANAGER_H
#define PROFILE_MANAGER_H

#include "ChipProfile.h"

#include <QMap>
#include <QString>
#include <QVector>

#include <memory>

// Менеджер профилей чипов. Загружает *.json из каталога профилей.
// Поддерживает как одиночный объект, так и массив профилей в файле.
class ProfileManager
{
public:
    static ProfileManager &instance();

    bool loadProfiles(const QString &directoryPath);

    // Найти каталог профилей: настройки окружения, каталог установки, каталог сборки
    static QString defaultProfilesDirectory();

    // Загрузить из каталога по умолчанию
    bool loadDefaultProfiles();

    std::shared_ptr<ChipProfile> profile(const QString &id) const;
    QVector<QString> profileIds() const;
    QVector<std::shared_ptr<ChipProfile>> eepromProfiles() const;

    int count() const { return m_profiles.size(); }

private:
    ProfileManager() = default;

    bool loadFile(const QString &filePath);
    void addProfile(const ChipProfile &profile);

    QMap<QString, std::shared_ptr<ChipProfile>> m_profiles;
};

#endif // PROFILE_MANAGER_H
