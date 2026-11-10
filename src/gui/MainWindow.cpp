#include "MainWindow.h"

#include "core/CounterReset.h"
#include "core/DumpManager.h"
#include "core/ProfileManager.h"
#include "gui/CartridgePanel.h"
#include "gui/ConnectionPanel.h"
#include "gui/HexEditor.h"
#include "gui/LogPanel.h"
#include "gui/OperationsPanel.h"
#include "gui/StatusBar.h"
#include "utils/Logger.h"

#include <QCloseEvent>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QMetaObject>
#include <QSplitter>
#include <QStringList>
#include <QThread>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Resetter Chip — сброс счётчиков чипов картриджей"));
    setMinimumSize(1100, 720);

    setupUi();
    setupWorker();
    setupConnections();
    loadProfiles();

    setDumpAvailable(false);
    m_connectionPanel->setConnected(false);
    m_cartridgePanel->setInfoEnabled(false);
    m_operationsPanel->setBusy(false);
    updateActions();

    Logger::instance().info(QStringLiteral("Главное окно инициализировано"));
}

MainWindow::~MainWindow()
{
    if (m_worker) {
        m_worker->cancel();
    }

    if (m_workerThread) {
        m_workerThread->quit();
        if (!m_workerThread->wait(15000)) {
            Logger::instance().warning(
                QStringLiteral("Рабочий поток не завершился за 15 секунд, ожидание продолжено"));
            m_workerThread->wait();
        }
    }

    // ChipWorker удаляется через QThread::finished -> deleteLater
    m_worker = nullptr;

    Logger::instance().info(QStringLiteral("Завершение работы приложения"));
}

void MainWindow::setupUi()
{
    auto *central = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(central);

    m_connectionPanel = new ConnectionPanel(this);
    mainLayout->addWidget(m_connectionPanel);

    auto *splitter = new QSplitter(Qt::Vertical, this);

    auto *leftWidget = new QWidget(this);
    auto *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    m_cartridgePanel = new CartridgePanel(this);
    leftLayout->addWidget(m_cartridgePanel);

    m_operationsPanel = new OperationsPanel(this);
    leftLayout->addWidget(m_operationsPanel);
    leftLayout->addStretch();

    splitter->addWidget(leftWidget);

    m_hexEditor = new HexEditor(this);
    splitter->addWidget(m_hexEditor);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);

    mainLayout->addWidget(splitter, 1);

    m_logPanel = new LogPanel(this);
    m_logPanel->setMinimumHeight(120);
    mainLayout->addWidget(m_logPanel);

    m_statusBar = new StatusBar(this);
    setStatusBar(m_statusBar);

    setCentralWidget(central);
}

void MainWindow::setupWorker()
{
    m_workerThread = new QThread(this);
    m_worker = new ChipWorker();
    m_worker->moveToThread(m_workerThread);

    connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &ChipWorker::operationResult,
            this, &MainWindow::onWorkerResult, Qt::QueuedConnection);
    connect(m_worker, &ChipWorker::operationProgress,
            this, &MainWindow::onWorkerProgress, Qt::QueuedConnection);
    connect(m_worker, &ChipWorker::operationFinished,
            this, &MainWindow::onWorkerFinished, Qt::QueuedConnection);

    m_workerThread->start();
}

void MainWindow::setupConnections()
{
    connect(m_connectionPanel, &ConnectionPanel::connectRequested,
            this, &MainWindow::onConnectRequested);
    connect(m_connectionPanel, &ConnectionPanel::disconnectRequested,
            this, &MainWindow::onDisconnectRequested);
    connect(m_connectionPanel, &ConnectionPanel::scanRequested,
            this, &MainWindow::onScanRequested);
    connect(m_connectionPanel, &ConnectionPanel::profileChanged,
            this, &MainWindow::onProfileChanged);

    connect(m_operationsPanel, &OperationsPanel::readChipRequested,
            this, &MainWindow::onReadRequested);
    connect(m_operationsPanel, &OperationsPanel::writeChipRequested,
            this, &MainWindow::onWriteRequested);
    connect(m_operationsPanel, &OperationsPanel::verifyWriteRequested,
            this, &MainWindow::onVerifyRequested);
    connect(m_operationsPanel, &OperationsPanel::resetCounterRequested,
            this, &MainWindow::onResetCounterRequested);
    connect(m_operationsPanel, &OperationsPanel::backupRequested,
            this, &MainWindow::onBackupRequested);
    connect(m_operationsPanel, &OperationsPanel::restoreRequested,
            this, &MainWindow::onRestoreRequested);
    connect(m_operationsPanel, &OperationsPanel::compareDumpsRequested,
            this, &MainWindow::onCompareRequested);
    connect(m_operationsPanel, &OperationsPanel::saveDumpRequested,
            this, &MainWindow::onSaveDumpRequested);
    connect(m_operationsPanel, &OperationsPanel::openDumpRequested,
            this, &MainWindow::onOpenDumpRequested);
    connect(m_operationsPanel, &OperationsPanel::cancelRequested,
            this, &MainWindow::onCancelRequested);

    connect(m_hexEditor, &HexEditor::dataChanged,
            this, &MainWindow::onHexDataChanged);
}

void MainWindow::loadProfiles()
{
    ProfileManager &manager = ProfileManager::instance();
    if (manager.count() == 0 && manager.cartridgeCount() == 0) {
        manager.loadDefaultProfiles();
    }

    QStringList chipIds;
    for (const QString &id : manager.profileIds()) {
        chipIds << id;
    }
    chipIds.sort();

    QStringList cartridgeIds;
    for (const QString &id : manager.cartridgeProfileIds()) {
        cartridgeIds << id;
    }
    cartridgeIds.sort();

    if (chipIds.isEmpty() && cartridgeIds.isEmpty()) {
        m_logPanel->addMessage(
            QStringLiteral("Профили не загружены — операции недоступны. "
                           "Проверьте каталог профилей или переменную RESETTER_CHIP_PROFILES"),
            "error");
        updateActions();
        return;
    }

    m_connectionPanel->setProfiles(chipIds, cartridgeIds);
    m_profileId = m_connectionPanel->selectedProfile();
    updateProfileInfo();
    updateTitle();
    updateActions();

    m_logPanel->addMessage(QStringLiteral("Загружено профилей: чипов %1, картриджей %2")
                               .arg(chipIds.size())
                               .arg(cartridgeIds.size()));
}

void MainWindow::updateTitle()
{
    QString title = QStringLiteral("Resetter Chip");
    if (!m_profileId.isEmpty()) {
        title += QStringLiteral(" — %1").arg(m_profileId);
    }
    title += m_connected ? QStringLiteral(" — подключено") : QStringLiteral(" — не подключено");
    setWindowTitle(title);
}

void MainWindow::updateProfileInfo()
{
    ProfileManager &manager = ProfileManager::instance();
    const auto cartridge = manager.cartridgeProfile(m_profileId);

    if (cartridge) {
        QString model = cartridge->cartridgeModels.value(0);
        if (model.isEmpty()) {
            model = cartridge->name;
        }
        m_cartridgePanel->setCartridgeModel(model);
        m_cartridgePanel->setChipId(cartridge->chipProfileId);
    } else {
        m_cartridgePanel->setCartridgeModel(QString());
        m_cartridgePanel->setChipId(m_profileId);
    }

    const auto resolved = manager.resolveProfile(m_profileId);
    if (resolved && !m_connected) {
        m_connectionPanel->setChipAddress(resolved->i2cAddress);
    }
}

void MainWindow::updateActions()
{
    const auto resolved = ProfileManager::instance().resolveProfile(currentProfileId());
    const bool hasCounter = resolved && resolved->counterOffset != 0xFFFFFFFFu
                            && resolved->counterSize > 0;
    m_operationsPanel->setState(m_connected, m_hasDump, hasCounter,
                                dumpMatchesCurrentProfile());
}

void MainWindow::setBusy(bool busy)
{
    m_busy = busy;
    m_connectionPanel->setBusy(busy);
    m_operationsPanel->setBusy(busy);
}

void MainWindow::showError(const QString &title, const QString &message)
{
    m_logPanel->addMessage(QStringLiteral("%1: %2").arg(title, message), "error");
    m_statusBar->showError(message);
    Logger::instance().error(QStringLiteral("%1: %2").arg(title, message));
    QMessageBox::critical(this, title, message);
}

QString MainWindow::currentProfileId() const
{
    return m_profileId.isEmpty() ? m_connectionPanel->selectedProfile() : m_profileId;
}

bool MainWindow::dumpMatchesCurrentProfile() const
{
    if (!m_hasDump) {
        return false;
    }
    const auto profile = ProfileManager::instance().resolveProfile(currentProfileId());
    return profile && static_cast<quint32>(m_currentDump.size()) == profile->sizeBytes;
}

void MainWindow::setDumpAvailable(bool available)
{
    m_hasDump = available;
    m_statusBar->setDumpLoaded(available, m_currentDump.size());
    updateActions();
}

void MainWindow::applyDump(const QByteArray &dump, const QString &chipId)
{
    m_currentDump = dump;
    m_hexEditor->setDump(dump);

    if (!chipId.isEmpty()) {
        m_cartridgePanel->setChipId(chipId);
    }

    setDumpAvailable(!dump.isEmpty());
    m_statusBar->setModified(false);
    updateChipInfoFromDump(dump);
}

void MainWindow::selectProfileForDump(const QByteArray &dump, const QString &preferredProfileId)
{
    ProfileManager &manager = ProfileManager::instance();

    if (!preferredProfileId.isEmpty()) {
        const auto preferred = manager.resolveProfile(preferredProfileId);
        if (preferred && static_cast<quint32>(dump.size()) == preferred->sizeBytes
            && m_connectionPanel->selectProfile(preferredProfileId)) {
            m_profileId = preferredProfileId;
            updateProfileInfo();
            updateTitle();
            return;
        }
    }

    if (dumpMatchesCurrentProfile()) {
        return;
    }

    QStringList candidates;
    for (const auto &profile : manager.eepromProfiles()) {
        if (static_cast<quint32>(dump.size()) == profile->sizeBytes) {
            candidates << profile->id;
        }
    }
    candidates.sort();

    if (candidates.size() == 1 && m_connectionPanel->selectProfile(candidates.first())) {
        m_profileId = candidates.first();
        updateProfileInfo();
        updateTitle();
        m_logPanel->addMessage(
            QStringLiteral("По размеру дампа автоматически выбран профиль %1").arg(m_profileId),
            "info");
        return;
    }

    if (!candidates.isEmpty()) {
        m_logPanel->addMessage(
            QStringLiteral("Дамп %1 байт подходит к профилям: %2. Выберите нужный вручную.")
                .arg(dump.size())
                .arg(candidates.join(QStringLiteral(", "))),
            "warning");
    } else {
        m_logPanel->addMessage(
            QStringLiteral("Для дампа размером %1 байт нет подходящего профиля чипа")
                .arg(dump.size()),
            "warning");
    }
}

void MainWindow::onProfileChanged(const QString &profileId)
{
    m_profileId = profileId;
    updateProfileInfo();
    updateTitle();
    if (m_hasDump) {
        updateChipInfoFromDump(m_currentDump);
    }
    updateActions();
}

void MainWindow::onConnectRequested()
{
    if (m_busy) {
        return;
    }

    const QString profileId = currentProfileId();
    if (profileId.isEmpty()) {
        showError(QStringLiteral("Ошибка"), QStringLiteral("Не выбран профиль чипа или картриджа"));
        return;
    }

    QString specError;
    const QString adapterSpec = m_connectionPanel->adapterSpec(&specError);
    if (adapterSpec.isEmpty()) {
        showError(QStringLiteral("Ошибка подключения"), specError);
        return;
    }

    setBusy(true);
    m_statusBar->setProgress(0, QStringLiteral("Подключение программатора..."));
    m_logPanel->addMessage(QStringLiteral("Подключение: %1; профиль %2; адрес 0x%3")
                               .arg(adapterSpec, profileId)
                               .arg(m_connectionPanel->chipAddress(), 2, 16, QLatin1Char('0')));

    QMetaObject::invokeMethod(m_worker, "requestConnect", Qt::QueuedConnection,
                              Q_ARG(QString, adapterSpec),
                              Q_ARG(int, static_cast<int>(m_connectionPanel->chipAddress())),
                              Q_ARG(QString, profileId));
}

void MainWindow::onDisconnectRequested()
{
    if (m_busy) {
        return;
    }
    setBusy(true);
    QMetaObject::invokeMethod(m_worker, "requestDisconnect", Qt::QueuedConnection);
}

void MainWindow::onScanRequested()
{
    if (m_busy || !m_connected) {
        return;
    }
    setBusy(true);
    m_logPanel->addMessage(QStringLiteral("Сканирование шины I2C..."));
    QMetaObject::invokeMethod(m_worker, "requestScan", Qt::QueuedConnection,
                              Q_ARG(QString, currentProfileId()));
}

void MainWindow::onReadRequested()
{
    if (m_busy || !m_connected) {
        if (!m_connected) {
            showError(QStringLiteral("Ошибка"), QStringLiteral("Программатор не подключён"));
        }
        return;
    }

    setBusy(true);
    m_statusBar->setProgress(0, QStringLiteral("Чтение чипа..."));
    m_logPanel->addMessage(QStringLiteral("Чтение чипа %1...").arg(currentProfileId()));

    QMetaObject::invokeMethod(m_worker, "requestRead", Qt::QueuedConnection,
                              Q_ARG(QString, currentProfileId()));
}

void MainWindow::onWriteRequested()
{
    if (m_busy || !m_connected) {
        if (!m_connected) {
            showError(QStringLiteral("Ошибка"), QStringLiteral("Программатор не подключён"));
        }
        return;
    }
    if (!m_hasDump) {
        showError(QStringLiteral("Ошибка"), QStringLiteral("Нет дампа для записи"));
        return;
    }
    if (!dumpMatchesCurrentProfile()) {
        showError(QStringLiteral("Ошибка"),
                  QStringLiteral("Размер дампа не соответствует выбранному профилю %1")
                      .arg(currentProfileId()));
        return;
    }

    const QByteArray data = m_hexEditor->dump();
    const auto answer = QMessageBox::question(
        this, QStringLiteral("Подтверждение записи"),
        QStringLiteral("Записать в чип %1 байт (профиль %2)?\n\n"
                       "Перед записью исходное содержимое чипа будет прочитано и сохранено "
                       "в автоматическую резервную копию.\n"
                       "После записи данные будут проверены автоматически.")
            .arg(data.size())
            .arg(currentProfileId()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (answer != QMessageBox::Yes) {
        return;
    }

    setBusy(true);
    m_statusBar->setProgress(0, QStringLiteral("Запись в чип..."));
    m_logPanel->addMessage(QStringLiteral("Запись в чип %1...").arg(currentProfileId()));

    QMetaObject::invokeMethod(m_worker, "requestWrite", Qt::QueuedConnection,
                              Q_ARG(QByteArray, data),
                              Q_ARG(QString, currentProfileId()));
}

void MainWindow::onVerifyRequested()
{
    if (m_busy || !m_connected) {
        return;
    }
    if (!m_hasDump || !dumpMatchesCurrentProfile()) {
        showError(QStringLiteral("Ошибка"),
                  QStringLiteral("Нет дампа, соответствующего выбранному профилю"));
        return;
    }

    setBusy(true);
    m_statusBar->setProgress(0, QStringLiteral("Проверка записи..."));
    m_logPanel->addMessage(QStringLiteral("Верификация содержимого чипа..."));

    QMetaObject::invokeMethod(m_worker, "requestVerify", Qt::QueuedConnection,
                              Q_ARG(QByteArray, m_hexEditor->dump()),
                              Q_ARG(QString, currentProfileId()));
}

void MainWindow::onResetCounterRequested()
{
    if (m_busy) {
        return;
    }
    if (!m_hasDump || !dumpMatchesCurrentProfile()) {
        showError(QStringLiteral("Ошибка"), QStringLiteral("Сначала прочитайте дамп чипа"));
        return;
    }

    const auto profile = ProfileManager::instance().resolveProfile(currentProfileId());
    if (!profile || profile->counterOffset == 0xFFFFFFFFu) {
        showError(QStringLiteral("Ошибка"),
                  QStringLiteral("В профиле %1 не задано смещение счётчика")
                      .arg(currentProfileId()));
        return;
    }

    setBusy(true);
    m_logPanel->addMessage(QStringLiteral("Обнуление счётчика %1...").arg(currentProfileId()));
    QMetaObject::invokeMethod(m_worker, "requestResetCounter", Qt::QueuedConnection,
                              Q_ARG(QByteArray, m_hexEditor->dump()),
                              Q_ARG(QString, currentProfileId()));
}

void MainWindow::onBackupRequested()
{
    if (m_busy) {
        return;
    }
    if (!m_hasDump) {
        showError(QStringLiteral("Ошибка"), QStringLiteral("Нет данных для резервного копирования"));
        return;
    }

    setBusy(true);
    QMetaObject::invokeMethod(m_worker, "requestBackup", Qt::QueuedConnection,
                              Q_ARG(QByteArray, m_hexEditor->dump()),
                              Q_ARG(QString, currentProfileId()),
                              Q_ARG(QString, m_cartridgePanel->cartridgeModel()));
}

void MainWindow::onRestoreRequested()
{
    if (m_busy) {
        return;
    }

    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Открыть резервную копию"),
        DumpManager::backupDirectory(),
        QStringLiteral("Дампы Resetter (*.rchip);;Сырые дампы (*.bin);;Все файлы (*)"));
    if (path.isEmpty()) {
        return;
    }

    setBusy(true);
    m_logPanel->addMessage(QStringLiteral("Загрузка резервной копии: %1").arg(path));
    QMetaObject::invokeMethod(m_worker, "requestRestore", Qt::QueuedConnection,
                              Q_ARG(QString, path),
                              Q_ARG(QString, currentProfileId()));
}

void MainWindow::onCompareRequested()
{
    if (m_busy) {
        return;
    }
    if (!m_hasDump) {
        showError(QStringLiteral("Ошибка"), QStringLiteral("Сначала прочитайте или откройте дамп"));
        return;
    }

    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Выберите дамп для сравнения"), QString(),
        QStringLiteral("Дампы Resetter (*.rchip);;Сырые дампы (*.bin);;Все файлы (*)"));
    if (path.isEmpty()) {
        return;
    }

    const auto other = DumpManager::loadDump(path);
    if (!other.has_value()) {
        showError(QStringLiteral("Ошибка"), QStringLiteral("Не удалось прочитать файл: %1").arg(path));
        return;
    }

    setBusy(true);
    QMetaObject::invokeMethod(m_worker, "requestCompare", Qt::QueuedConnection,
                              Q_ARG(QByteArray, m_hexEditor->dump()),
                              Q_ARG(QByteArray, *other));
}

void MainWindow::onSaveDumpRequested()
{
    if (m_busy) {
        return;
    }
    if (!m_hasDump) {
        showError(QStringLiteral("Ошибка"), QStringLiteral("Нет дампа для сохранения"));
        return;
    }

    const QString defaultName = QStringLiteral("%1_dump.rchip")
                                    .arg(currentProfileId().isEmpty() ? QStringLiteral("chip")
                                                                      : currentProfileId());
    QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Сохранить дамп"), defaultName,
        QStringLiteral("Контейнер Resetter (*.rchip);;Сырой бинарный дамп (*.bin);;Все файлы (*)"));
    if (path.isEmpty()) {
        return;
    }

    if (QFileInfo(path).suffix().isEmpty()) {
        path += QStringLiteral(".rchip");
    }

    const bool raw = QFileInfo(path).suffix().compare(QStringLiteral("bin"), Qt::CaseInsensitive) == 0;
    const bool saved = raw
                           ? DumpManager::saveRawDump(path, m_hexEditor->dump())
                           : DumpManager::saveDump(path, m_hexEditor->dump(),
                                                   currentProfileId(),
                                                   m_cartridgePanel->cartridgeModel());
    if (saved) {
        m_logPanel->addMessage(
            QStringLiteral("Дамп сохранён (%1): %2")
                .arg(raw ? QStringLiteral("сырой .bin") : QStringLiteral("контейнер .rchip"), path),
            "success");
    } else {
        showError(QStringLiteral("Ошибка"), QStringLiteral("Не удалось сохранить дамп: %1").arg(path));
    }
}

void MainWindow::onOpenDumpRequested()
{
    if (m_busy) {
        return;
    }

    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Открыть дамп"), QString(),
        QStringLiteral("Дампы Resetter (*.rchip);;Сырые дампы (*.bin);;Все файлы (*)"));
    if (path.isEmpty()) {
        return;
    }

    const auto data = DumpManager::loadDump(path);
    if (!data.has_value()) {
        showError(QStringLiteral("Ошибка"), QStringLiteral("Не удалось прочитать файл: %1").arg(path));
        return;
    }

    const auto info = DumpManager::dumpInfo(path);
    const QString preferred = info.has_value() ? info->chipId : QString();
    selectProfileForDump(*data, preferred);

    applyDump(*data, preferred.isEmpty() ? currentProfileId() : preferred);

    if (info.has_value() && !info->cartridgeModel.isEmpty()) {
        m_cartridgePanel->setCartridgeModel(info->cartridgeModel);
    }

    m_logPanel->addMessage(QStringLiteral("Дамп загружен: %1 (%2 байт)")
                               .arg(path)
                               .arg(data->size()),
                           "success");

    if (!dumpMatchesCurrentProfile()) {
        QMessageBox::warning(
            this, QStringLiteral("Профиль не совпадает"),
            QStringLiteral("Размер дампа (%1 байт) не совпадает с профилем %2. "
                           "Просмотр и редактирование доступны, но запись и проверка отключены. "
                           "Выберите подходящий профиль чипа.")
                .arg(data->size())
                .arg(currentProfileId()));
    }
}

void MainWindow::onCancelRequested()
{
    if (m_worker) {
        m_worker->cancel();
        m_logPanel->addMessage(QStringLiteral("Запрошена отмена операции"), "warning");
    }
}

void MainWindow::onHexDataChanged(const QByteArray &data)
{
    m_currentDump = data;
    m_statusBar->setModified(true);
    setDumpAvailable(!data.isEmpty());
}

void MainWindow::onWorkerResult(const ChipOperationResult &result)
{
    const bool ok = (result.error == ErrorCode::Success);

    if (result.error == ErrorCode::OperationCancelled) {
        m_logPanel->addMessage(QStringLiteral("Операция отменена"), "warning");
        m_statusBar->showError(QStringLiteral("Операция отменена"));
        return;
    }

    if (!result.backupPath.isEmpty()) {
        m_logPanel->addMessage(QStringLiteral("Резервная копия: %1").arg(result.backupPath), "info");
    }

    switch (result.type) {
    case ChipOperationType::Connect:
        if (ok) {
            m_connected = true;
            m_connectionPanel->setConnected(true);
            m_connectionPanel->showStatusMessage(QStringLiteral("Статус: подключено"), true);
            m_statusBar->setAdapterState(true);
            m_cartridgePanel->setInfoEnabled(true);
            m_logPanel->addMessage(result.progressText, "success");
            updateProfileInfo();
            updateTitle();
            updateActions();
        } else {
            m_connected = false;
            m_connectionPanel->setConnected(false);
            m_connectionPanel->showStatusMessage(QStringLiteral("Статус: ошибка подключения"), false);
            m_statusBar->setAdapterState(false);
            updateActions();
            showError(QStringLiteral("Ошибка подключения"), result.errorMessage);
        }
        break;

    case ChipOperationType::Disconnect:
        m_connected = false;
        m_connectionPanel->setConnected(false);
        m_connectionPanel->showStatusMessage(QStringLiteral("Статус: не подключено"), false);
        m_statusBar->setAdapterState(false);
        m_cartridgePanel->setInfoEnabled(false);
        m_logPanel->addMessage(result.progressText, "info");
        updateTitle();
        updateActions();
        break;

    case ChipOperationType::Scan:
        if (ok) {
            m_logPanel->addMessage(result.progressText,
                                   result.detectedAddresses.isEmpty() ? "warning" : "success");
            if (!result.detectedAddresses.isEmpty()) {
                QStringList addresses;
                for (quint8 address : result.detectedAddresses) {
                    addresses << QStringLiteral("0x%1").arg(address, 2, 16, QLatin1Char('0'));
                }
                m_logPanel->addMessage(QStringLiteral("Адреса устройств: %1")
                                           .arg(addresses.join(QStringLiteral(", "))),
                                       "info");
            }
        } else {
            showError(QStringLiteral("Ошибка сканирования"), result.errorMessage);
        }
        break;

    case ChipOperationType::Read:
        if (ok) {
            applyDump(result.data, currentProfileId());
            m_logPanel->addMessage(result.progressText, "success");
        } else {
            showError(QStringLiteral("Ошибка чтения"), result.errorMessage);
        }
        break;

    case ChipOperationType::Write:
        if (ok) {
            m_statusBar->setModified(false);
            // Сбрасываем базовую линию подсветки: чип теперь содержит этот дамп.
            m_hexEditor->setDump(m_hexEditor->dump());
            m_logPanel->addMessage(result.progressText, "success");
            QMessageBox::information(
                this, QStringLiteral("Запись завершена"),
                result.progressText
                    + QStringLiteral("\n\nДанные записаны и автоматически проверены."));
        } else if (!result.differences.isEmpty()) {
            showError(QStringLiteral("Верификация не пройдена"),
                      QStringLiteral("%1\nПервое различие по адресу 0x%2")
                          .arg(result.errorMessage)
                          .arg(result.differences.first(), 4, 16, QLatin1Char('0')));
        } else {
            showError(QStringLiteral("Ошибка записи"), result.errorMessage);
        }
        break;

    case ChipOperationType::Verify:
        if (ok) {
            m_logPanel->addMessage(result.progressText, "success");
            QMessageBox::information(this, QStringLiteral("Проверка"), result.progressText);
        } else if (!result.differences.isEmpty()) {
            m_logPanel->addMessage(result.errorMessage, "error");
            showError(QStringLiteral("Верификация не пройдена"),
                      QStringLiteral("%1\nПервое различие по адресу 0x%2")
                          .arg(result.errorMessage)
                          .arg(result.differences.first(), 4, 16, QLatin1Char('0')));
        } else {
            showError(QStringLiteral("Ошибка проверки"), result.errorMessage);
        }
        break;

    case ChipOperationType::ResetCounter:
        if (ok) {
            applyDump(result.data, currentProfileId());
            m_logPanel->addMessage(result.progressText, "success");
            QMessageBox::information(
                this, QStringLiteral("Счётчик обнулён"),
                result.progressText
                    + QStringLiteral("\n\nДамп изменён в памяти. Чтобы применить изменения "
                                     "к чипу, нажмите «Записать в чип»."));
        } else {
            showError(QStringLiteral("Ошибка сброса счётчика"), result.errorMessage);
        }
        break;

    case ChipOperationType::Backup:
        if (ok) {
            m_logPanel->addMessage(result.progressText, "success");
            QMessageBox::information(this, QStringLiteral("Резервная копия"),
                                     result.progressText);
        } else {
            showError(QStringLiteral("Ошибка резервного копирования"), result.errorMessage);
        }
        break;

    case ChipOperationType::Restore:
        if (ok) {
            applyDump(result.data, currentProfileId());
            m_statusBar->setModified(true);
            m_logPanel->addMessage(result.progressText, "success");
            QMessageBox::information(
                this, QStringLiteral("Копия загружена"),
                result.progressText
                    + QStringLiteral("\n\nДанные загружены в редактор, но ещё не записаны в чип. "
                                     "Проверьте их и нажмите «Записать в чип»."));
        } else {
            showError(QStringLiteral("Ошибка восстановления"), result.errorMessage);
        }
        break;

    case ChipOperationType::Compare:
        if (ok) {
            m_logPanel->addMessage(result.progressText, "info");
            QMessageBox::information(this, QStringLiteral("Сравнение дампов"), result.progressText);
        } else {
            showError(QStringLiteral("Ошибка сравнения"), result.errorMessage);
        }
        break;
    }

    updateActions();
}

void MainWindow::onWorkerProgress(int value, const QString &text)
{
    m_statusBar->setProgress(value, text);
}

void MainWindow::onWorkerFinished()
{
    setBusy(false);
    m_statusBar->setProgress(0, QString());
    updateActions();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_busy) {
        const auto answer = QMessageBox::question(
            this, QStringLiteral("Операция выполняется"),
            QStringLiteral("Операция с чипом ещё не завершена. Прервать и закрыть программу?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            event->ignore();
            return;
        }
        if (m_worker) {
            m_worker->cancel();
        }
    }

    event->accept();
}

void MainWindow::updateChipInfoFromDump(const QByteArray &dump)
{
    const QString profileId = currentProfileId();
    const auto profile = ProfileManager::instance().resolveProfile(profileId);

    if (!profile) {
        m_cartridgePanel->updateFromDump(dump, profileId, QString(),
                                         static_cast<quint32>(dump.size()), false, 0);
        return;
    }

    const auto counter = CounterReset::readCounter(dump, *profile);
    m_cartridgePanel->updateFromDump(dump, profile->id, profile->name, profile->sizeBytes,
                                     counter.has_value(), counter.value_or(0));
}
