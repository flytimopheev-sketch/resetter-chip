#include "HexEditor.h"

#include <QKeyEvent>
#include <QRegularExpression>
#include <QStringList>
#include <QTextCursor>

#include <cctype>

namespace {
constexpr int kBytesPerLine = 16;
constexpr int kHexAreaWidth = kBytesPerLine * 3;  // "XX " на байт

// Признак допустимого символа hex-разряда
inline bool isHexDigit(QChar c)
{
    return c.isDigit() || (c >= QLatin1Char('a') && c <= QLatin1Char('f'))
           || (c >= QLatin1Char('A') && c <= QLatin1Char('F'));
}
}

HexEditor::HexEditor(QWidget *parent)
    : QPlainTextEdit(parent)
{
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setFont(QFont(QStringLiteral("Monospace"), 10));
    setStyleSheet(QStringLiteral("QPlainTextEdit { background-color: #1e1e1e; color: #d4d4d4; }"));
    setTabChangesFocus(true);

    connect(this, &QPlainTextEdit::textChanged, this, &HexEditor::onTextChanged);
}

void HexEditor::clearDump()
{
    m_originalData.clear();
    m_currentData.clear();
    m_updating = true;
    clear();
    m_updating = false;
}

void HexEditor::setDump(const QByteArray &data)
{
    m_originalData = data;
    m_currentData = data;
    rebuildText();
}

QByteArray HexEditor::dump() const
{
    return m_currentData;
}

void HexEditor::highlightChanges(const QByteArray &original)
{
    m_originalData = original;
    rebuildText();
}

void HexEditor::rebuildText()
{
    QString text;
    text.reserve(m_currentData.size() * 4);

    for (int offset = 0; offset < m_currentData.size(); offset += kBytesPerLine) {
        const int chunk = qMin(kBytesPerLine, m_currentData.size() - offset);

        QString hexPart;
        QString asciiPart;

        for (int i = 0; i < kBytesPerLine; ++i) {
            if (i < chunk) {
                const uchar byte = static_cast<uchar>(m_currentData.at(offset + i));
                const bool changed = (offset + i) < m_originalData.size()
                                     && m_originalData.at(offset + i) != m_currentData.at(offset + i);

                QString token = QStringLiteral("%1").arg(byte, 2, 16, QLatin1Char('0'));
                token = changed ? token.toLower() : token.toUpper();
                hexPart += token + QLatin1Char(' ');

                asciiPart += (byte >= 32 && byte < 127) ? QChar(byte) : QLatin1Char('.');
            } else {
                hexPart += QStringLiteral("   ");
            }
        }

        text += QStringLiteral("%1:  %2 |%3|\n")
                    .arg(static_cast<qulonglong>(offset), 8, 16, QLatin1Char('0'))
                    .arg(hexPart)
                    .arg(asciiPart);
    }

    m_updating = true;
    const int cursorPos = textCursor().position();
    setPlainText(text);
    QTextCursor cursor = textCursor();
    cursor.setPosition(qMin(cursorPos, toPlainText().size()));
    setTextCursor(cursor);
    m_updating = false;
}

bool HexEditor::parseText(const QString &text, QByteArray &out) const
{
    out.clear();
    const QStringList lines = text.split(QLatin1Char('\n'));

    for (const QString &line : lines) {
        const int colon = line.indexOf(QLatin1Char(':'));
        if (colon < 0) {
            continue;
        }

        // Смещение строки
        bool ok = false;
        const uint offset = line.left(colon).trimmed().toUInt(&ok, 16);
        if (!ok) {
            return false;
        }

        // Hex-часть: между ':' и '|'
        int end = line.indexOf(QLatin1Char('|'), colon);
        if (end < 0) {
            end = line.size();
        }
        const QString hexArea = line.mid(colon + 1, end - colon - 1);

        // Разбиваем по пробелам на токены по 2 hex-символа
        QByteArray bytes;
        QString token;
        for (int i = 0; i <= hexArea.size(); ++i) {
            const QChar c = (i < hexArea.size()) ? hexArea.at(i) : QLatin1Char(' ');
            if (isHexDigit(c)) {
                token += c;
            } else {
                if (token.size() == 2) {
                    bytes.append(static_cast<char>(token.toUInt(nullptr, 16) & 0xFF));
                } else if (!token.isEmpty()) {
                    return false;  // повреждённый токен
                }
                token.clear();
            }
        }

        if (bytes.isEmpty()) {
            continue;
        }

        // Записываем по указанному смещению
        if (out.size() < offset + bytes.size()) {
            out.resize(static_cast<int>(offset + bytes.size()));
        }
        for (int i = 0; i < bytes.size(); ++i) {
            out[static_cast<int>(offset) + i] = bytes.at(i);
        }
    }

    return true;
}

void HexEditor::onTextChanged()
{
    if (m_updating) {
        return;
    }

    QByteArray parsed;
    if (!parseText(toPlainText(), parsed)) {
        return;  // некорректный ввод игнорируем
    }

    if (parsed.size() != m_currentData.size()) {
        return;  // размер дампа менять нельзя
    }

    if (parsed == m_currentData) {
        return;
    }

    m_currentData = parsed;
    emit dataChanged(m_currentData);
}

void HexEditor::keyPressEvent(QKeyEvent *event)
{
    QPlainTextEdit::keyPressEvent(event);
}


