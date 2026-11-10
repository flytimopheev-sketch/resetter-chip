#include "SerialI2cAdapter.h"

#include <QElapsedTimer>
#include <QStringList>
#include <QThread>

#include <algorithm>
#include <cstring>

#ifdef Q_OS_UNIX
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace {
constexpr int kSerialTimeoutMs = 2500;
constexpr int kReadAttempts = 2;
constexpr int kWriteAttempts = 2;
constexpr int kMaxChunk = SerialProtocol::kMaxChunk;

QString statusText(SerialProtocol::Status status)
{
    switch (status) {
    case SerialProtocol::Status::Ok:
        return QStringLiteral("успешно");
    case SerialProtocol::Status::Nak:
        return QStringLiteral("чип не подтвердил транзакцию (NAK)");
    case SerialProtocol::Status::BadRequest:
        return QStringLiteral("неверный запрос");
    case SerialProtocol::Status::BusError:
        return QStringLiteral("ошибка шины I2C");
    case SerialProtocol::Status::Unsupported:
        return QStringLiteral("команда не поддерживается прошивкой");
    }
    return QStringLiteral("неизвестная ошибка");
}
} // namespace

SerialI2cAdapter::SerialI2cAdapter(const QString &portName)
    : m_portName(portName)
{
}

SerialI2cAdapter::~SerialI2cAdapter()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    closeUnlocked();
}

void SerialI2cAdapter::setError(const std::string &message)
{
    m_lastError = message;
}

void SerialI2cAdapter::closeUnlocked()
{
#ifdef Q_OS_UNIX
    if (m_fd >= 0) {
        ::close(m_fd);
    }
#endif
    m_fd = -1;
    m_configured = false;
}

bool SerialI2cAdapter::isOpen() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_fd >= 0;
}

void SerialI2cAdapter::setChipParams(const ChipParams &params)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_params = params;
    m_configured = false;
}

SerialProtocol::ChipConfig SerialI2cAdapter::chipConfig() const
{
    SerialProtocol::ChipConfig config;
    config.deviceAddress = m_params.deviceAddress;
    config.twoByteAddress = m_params.twoByteAddress;
    config.blockBits = m_params.blockBits;
    config.pageSize = m_params.pageSize;
    config.writeCycleMs = static_cast<quint16>(std::min<quint32>(m_params.writeCycleMs, 65535));
    return config;
}

bool SerialI2cAdapter::open()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_fd >= 0) {
        return true;
    }

#ifndef Q_OS_UNIX
    setError("Последовательный адаптер поддерживается на Linux/macOS. "
             "На Windows используйте CH341A или CH341A-совместимую плату.");
    return false;
#else
    QString port = m_portName.trimmed();
    if (port.isEmpty()) {
        const QStringList candidates = {
            QStringLiteral("/dev/ttyUSB0"),
            QStringLiteral("/dev/ttyACM0"),
            QStringLiteral("/dev/serial0")
        };
        for (const QString &candidate : candidates) {
            if (access(candidate.toLocal8Bit().constData(), R_OK | W_OK) == 0) {
                port = candidate;
                break;
            }
        }
    }

    if (port.isEmpty()) {
        setError("Не найден последовательный порт. Укажите /dev/ttyUSB0 или /dev/ttyACM0.");
        return false;
    }

    const QByteArray portBytes = port.toLocal8Bit();
    m_fd = ::open(portBytes.constData(), O_RDWR | O_NOCTTY | O_SYNC);
    if (m_fd < 0) {
        setError(QStringLiteral("Не удалось открыть порт %1: %2")
                     .arg(port, QString::fromLocal8Bit(std::strerror(errno)))
                     .toStdString());
        return false;
    }

    struct termios tio;
    if (tcgetattr(m_fd, &tio) != 0) {
        setError(QStringLiteral("Не удалось прочитать настройки порта %1: %2")
                     .arg(port, QString::fromLocal8Bit(std::strerror(errno)))
                     .toStdString());
        closeUnlocked();
        return false;
    }

    cfmakeraw(&tio);
    tio.c_cflag |= (CLOCAL | CREAD);
    tio.c_cflag &= ~CRTSCTS;
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;

    const speed_t speed = B115200;
    cfsetispeed(&tio, speed);
    cfsetospeed(&tio, speed);

    if (tcsetattr(m_fd, TCSANOW, &tio) != 0) {
        setError(QStringLiteral("Не удалось настроить порт %1: %2")
                     .arg(port, QString::fromLocal8Bit(std::strerror(errno)))
                     .toStdString());
        closeUnlocked();
        return false;
    }

    tcflush(m_fd, TCIOFLUSH);
    m_portName = port;

    // Проверяем, что на другой стороне прошивка Resetter, а не случайное устройство.
    QByteArray response;
    if (!sendRequestUnlocked(SerialProtocol::Command::Ping, 0, 0, 0, {}, response)
        || !QString::fromLatin1(response).startsWith(QStringLiteral("RESETTER"))) {
        closeUnlocked();
        if (m_lastError.empty()) {
            setError("Последовательный порт открыт, но прошивка Resetter не ответила на PING");
        }
        return false;
    }

    m_lastError.clear();
    return true;
#endif
}

void SerialI2cAdapter::close()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    closeUnlocked();
}

bool SerialI2cAdapter::ensureConfiguredUnlocked()
{
    if (m_configured) {
        return true;
    }

    const QByteArray payload = SerialProtocol::configurePayload(chipConfig());
    QByteArray response;
    if (!sendRequestUnlocked(SerialProtocol::Command::Configure, 0, 0,
                             static_cast<quint16>(payload.size()), payload, response)) {
        return false;
    }
    m_configured = true;
    return true;
}

bool SerialI2cAdapter::sendRequestUnlocked(SerialProtocol::Command command, quint8 i2cAddress,
                                           quint32 memoryAddress, quint16 length,
                                           const QByteArray &payload, QByteArray &responsePayload)
{
#ifndef Q_OS_UNIX
    Q_UNUSED(command)
    Q_UNUSED(i2cAddress)
    Q_UNUSED(memoryAddress)
    Q_UNUSED(length)
    Q_UNUSED(payload)
    Q_UNUSED(responsePayload)
    setError("Последовательный адаптер не поддерживается на этой платформе");
    return false;
#else
    if (m_fd < 0) {
        setError("Последовательный порт не открыт");
        return false;
    }

    const QByteArray request =
        SerialProtocol::buildRequest(command, i2cAddress, memoryAddress, length, payload);
    if (request.isEmpty()) {
        setError("Слишком длинный запрос к последовательному программатору");
        return false;
    }

    tcflush(m_fd, TCIFLUSH);
    if (!writeAllUnlocked(request)) {
        setError("Не удалось записать запрос в последовательный порт");
        return false;
    }

    char header[SerialProtocol::kResponseHeaderSize];
    if (!readExactUnlocked(header, SerialProtocol::kResponseHeaderSize, kSerialTimeoutMs)) {
        setError("Программатор не ответил (таймаут последовательного порта)");
        return false;
    }

    if (static_cast<quint8>(header[0]) != SerialProtocol::kMagic1
        || static_cast<quint8>(header[1]) != SerialProtocol::kMagic2) {
        setError("Неверная сигнатура ответа последовательного программатора");
        return false;
    }

    const int dataLength = static_cast<quint8>(header[4])
                           | (static_cast<int>(static_cast<quint8>(header[5])) << 8);
    if (dataLength < 0 || dataLength > SerialProtocol::kMaxPayload) {
        setError("Некорректная длина ответа последовательного программатора");
        return false;
    }

    QByteArray frame(header, SerialProtocol::kResponseHeaderSize);
    const int tailSize = dataLength + 2;
    if (tailSize > 0) {
        QByteArray tail(tailSize, '\0');
        if (!readExactUnlocked(tail.data(), tailSize, kSerialTimeoutMs)) {
            setError("Ответ последовательного программатора обрезан");
            return false;
        }
        frame.append(tail);
    }

    SerialProtocol::Status status = SerialProtocol::Status::BusError;
    SerialProtocol::Command responseCommand = SerialProtocol::Command::Ping;
    QString parseError;
    if (!SerialProtocol::parseResponse(frame, &status, &responseCommand, &responsePayload,
                                       &parseError)) {
        setError(parseError.toStdString());
        return false;
    }
    if (responseCommand != command) {
        setError("Последовательный программатор вернул ответ на другую команду");
        return false;
    }
    if (status != SerialProtocol::Status::Ok) {
        setError(QStringLiteral("Ошибка программатора: %1").arg(statusText(status)).toStdString());
        return false;
    }

    m_lastError.clear();
    return true;
#endif
}

bool SerialI2cAdapter::writeAllUnlocked(const QByteArray &data)
{
#ifndef Q_OS_UNIX
    Q_UNUSED(data)
    return false;
#else
    int written = 0;
    while (written < data.size()) {
        struct pollfd pfd;
        pfd.fd = m_fd;
        pfd.events = POLLOUT;
        pfd.revents = 0;
        const int ready = poll(&pfd, 1, kSerialTimeoutMs);
        if (ready <= 0) {
            return false;
        }

        const ssize_t rc = ::write(m_fd, data.constData() + written,
                                   static_cast<size_t>(data.size() - written));
        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        if (rc == 0) {
            return false;
        }
        written += static_cast<int>(rc);
    }
    return true;
#endif
}

bool SerialI2cAdapter::readExactUnlocked(char *buffer, int size, int timeoutMs)
{
#ifndef Q_OS_UNIX
    Q_UNUSED(buffer)
    Q_UNUSED(size)
    Q_UNUSED(timeoutMs)
    return false;
#else
    int received = 0;
    QElapsedTimer timer;
    timer.start();

    while (received < size) {
        const int remaining = timeoutMs - static_cast<int>(timer.elapsed());
        if (remaining <= 0) {
            return false;
        }

        struct pollfd pfd;
        pfd.fd = m_fd;
        pfd.events = POLLIN;
        pfd.revents = 0;
        const int ready = poll(&pfd, 1, remaining);
        if (ready <= 0) {
            return false;
        }
        if ((pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            return false;
        }

        const ssize_t rc = ::read(m_fd, buffer + received,
                                  static_cast<size_t>(size - received));
        if (rc < 0) {
            if (errno == EINTR || errno == EAGAIN) {
                continue;
            }
            return false;
        }
        if (rc == 0) {
            continue;
        }
        received += static_cast<int>(rc);
    }
    return true;
#endif
}

std::vector<uint8_t> SerialI2cAdapter::read(uint32_t address, uint32_t length)
{
    std::vector<uint8_t> result;

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_fd < 0) {
        setError("Последовательный программатор не открыт");
        return result;
    }
    if (length == 0) {
        return result;
    }
    if (!ensureConfiguredUnlocked()) {
        return result;
    }

    result.resize(length);

    uint32_t offset = 0;
    while (offset < length) {
        const uint32_t remaining = length - offset;
        const int chunk = static_cast<int>(std::min<uint32_t>(remaining, kMaxChunk));

        bool ok = false;
        for (int attempt = 0; attempt < kReadAttempts && !ok; ++attempt) {
            QByteArray response;
            ok = sendRequestUnlocked(SerialProtocol::Command::Read, m_params.deviceAddress,
                                     address + offset, static_cast<quint16>(chunk), {}, response)
                 && response.size() == chunk;
            if (ok) {
                std::memcpy(result.data() + offset, response.constData(),
                            static_cast<size_t>(chunk));
            } else {
                QThread::msleep(5);
            }
        }

        if (!ok) {
            result.clear();
            return result;
        }
        offset += static_cast<uint32_t>(chunk);
    }

    m_lastError.clear();
    return result;
}

bool SerialI2cAdapter::write(uint32_t address, const std::vector<uint8_t> &data)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_fd < 0) {
        setError("Последовательный программатор не открыт");
        return false;
    }
    if (data.empty()) {
        return true;
    }
    if (!ensureConfiguredUnlocked()) {
        return false;
    }

    const size_t pageSize = m_params.pageSize > 0 ? m_params.pageSize : 16;
    size_t offset = 0;
    while (offset < data.size()) {
        const uint32_t currentAddress = address + static_cast<uint32_t>(offset);
        const size_t pageRemaining = pageSize - (currentAddress % pageSize);
        size_t chunk = std::min<size_t>(data.size() - offset, pageRemaining);
        chunk = std::min<size_t>(chunk, kMaxChunk);

        const QByteArray payload(reinterpret_cast<const char *>(data.data() + offset),
                                 static_cast<int>(chunk));

        bool ok = false;
        for (int attempt = 0; attempt < kWriteAttempts && !ok; ++attempt) {
            QByteArray response;
            ok = sendRequestUnlocked(SerialProtocol::Command::Write, m_params.deviceAddress,
                                     currentAddress, static_cast<quint16>(chunk), payload,
                                     response);
            if (!ok) {
                QThread::msleep(5);
            }
        }

        if (!ok) {
            return false;
        }
        offset += chunk;
    }

    m_lastError.clear();
    return true;
}

std::vector<uint8_t> SerialI2cAdapter::scanBus()
{
    std::vector<uint8_t> found;

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_fd < 0) {
        setError("Последовательный программатор не открыт");
        return found;
    }

    QByteArray response;
    if (!sendRequestUnlocked(SerialProtocol::Command::Scan, 0, 0, 0, {}, response)) {
        return found;
    }

    for (char value : response) {
        const quint8 address = static_cast<quint8>(value);
        if (address >= 0x08 && address <= 0x77) {
            found.push_back(address);
        }
    }

    m_lastError.clear();
    return found;
}

std::string SerialI2cAdapter::lastError() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastError;
}

QString SerialI2cAdapter::adapterName() const
{
    if (m_portName.isEmpty()) {
        return QStringLiteral("Самодельный (последовательный порт)");
    }
    return QStringLiteral("Самодельный (последовательный: %1)").arg(m_portName);
}
