#include "HexEditor.h"

#include <QContextMenuEvent>
#include <QFocusEvent>
#include <QInputDialog>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLineEdit>
#include <QMenu>
#include <QRegularExpression>
#include <QScrollBar>
#include <QStringList>
#include <QTextBlock>
#include <QTextCursor>

#include <cctype>

namespace {
constexpr int kBytesPerLine = 16;
constexpr int kHexOffset = 11;          // начало hex-колонки в строке
constexpr int kHexTokenWidth = 3;       // "XX "
constexpr quint32 kMaxDumpSize = 16 * 1024 * 1024;

// Признак допустимого символа hex-разряда
inline bool isHexDigit(QChar c)
{
    return c.isDigit() || (c >= QLatin1Char('a') && c <= QLatin1Char('f'))
           || (c >= QLatin1Char('A') && c <= QLatin1Char('F'));
}

QByteArray parseHexSequence(const QString &text, bool *ok)
{
    QByteArray result;
    QString token;
    QString normalized = text;
    normalized.replace(QLatin1Char(','), QLatin1Char(' '));

    *ok = true;
    for (int i = 0; i <= normalized.size(); ++i) {
        const QChar c = (i < normalized.size()) ? normalized.at(i) : QLatin1Char(' ');
        if (isHexDigit(c)) {
            token += c;
        } else {
            if (token.size() == 2) {
                result.append(static_cast<char>(token.toUInt(nullptr, 16) & 0xFF));
            } else if (!token.isEmpty()) {
                *ok = false;
                return {};
            }
            token.clear();
        }
    }
    return result;
}
} // namespace

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

bool HexEditor::gotoOffset(quint32 offset)
{
    if (offset >= static_cast<quint32>(m_currentData.size())) {
        return false;
    }

    const int row = static_cast<int>(offset) / kBytesPerLine;
    const int column = static_cast<int>(offset) % kBytesPerLine;
    const QTextBlock block = document()->findBlockByNumber(row);
    if (!block.isValid()) {
        return false;
    }

    QTextCursor cursor(document());
    cursor.setPosition(block.position() + kHexOffset + column * kHexTokenWidth);
    cursor.setPosition(cursor.position() + 2, QTextCursor::KeepAnchor);
    setTextCursor(cursor);
    centerCursor();
    return true;
}

bool HexEditor::findBytes(const QByteArray &needle, quint32 fromOffset)
{
    if (needle.isEmpty() || m_currentData.isEmpty()) {
        return false;
    }

    const int start = static_cast<int>(qMin<quint32>(fromOffset,
                                                     static_cast<quint32>(m_currentData.size())));
    int index = m_currentData.indexOf(needle, start);
    if (index < 0 && start > 0) {
        index = m_currentData.indexOf(needle, 0);  // поиск с переноса
    }
    if (index < 0) {
        return false;
    }
    return gotoOffset(static_cast<quint32>(index));
}

void HexEditor::fill(quint8 value)
{
    if (m_currentData.isEmpty()) {
        return;
    }

    m_currentData.fill(static_cast<char>(value));
    rebuildText();
    emit dataChanged(m_currentData);
}

QString HexEditor::buildText() const
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
    return text;
}

void HexEditor::rebuildText()
{
    const int scroll = verticalScrollBar() ? verticalScrollBar()->value() : 0;
    const QString text = buildText();
    const int cursorPos = textCursor().position();

    m_updating = true;
    setPlainText(text);
    QTextCursor cursor = textCursor();
    cursor.setPosition(qMin(cursorPos, toPlainText().size()));
    setTextCursor(cursor);
    if (verticalScrollBar()) {
        verticalScrollBar()->setValue(scroll);
    }
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
        if (!ok || offset > kMaxDumpSize) {
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

        // Размер дампа менять нельзя: адрес и данные должны попадать
        // в уже загруженный буфер.
        if (offset + static_cast<uint>(bytes.size()) > static_cast<uint>(m_currentData.size())) {
            return false;
        }

        if (out.size() < static_cast<int>(offset) + bytes.size()) {
            out.resize(static_cast<int>(offset) + bytes.size());
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
        return;  // некорректный ввод игнорируем, пользователь дописывает разряд
    }

    if (parsed.size() != m_currentData.size()) {
        return;  // размер дампа менять нельзя
    }

    if (parsed == m_currentData) {
        // Правка только в ASCII-колонке или регистре hex не меняет данные.
        // Возвращаем канонический текст, чтобы отображение не расходилось с буфером.
        if (toPlainText() != buildText()) {
            rebuildText();
        }
        return;
    }

    m_currentData = parsed;
    emit dataChanged(m_currentData);
}

void HexEditor::keyPressEvent(QKeyEvent *event)
{
    if (event->matches(QKeySequence::Find)) {
        requestFind();
        event->accept();
        return;
    }

    if (event->modifiers().testFlag(Qt::ControlModifier)
        && event->key() == Qt::Key_G) {
        requestGoto();
        event->accept();
        return;
    }

    QPlainTextEdit::keyPressEvent(event);
}

void HexEditor::focusOutEvent(QFocusEvent *event)
{
    QByteArray parsed;
    if (!parseText(toPlainText(), parsed)
        || parsed.size() != m_currentData.size()
        || (parsed == m_currentData && toPlainText() != buildText())) {
        rebuildText();
    }
    QPlainTextEdit::focusOutEvent(event);
}

void HexEditor::requestGoto()
{
    bool ok = false;
    const QString text = QInputDialog::getText(
        this, QStringLiteral("Переход к адресу"),
        QStringLiteral("Адрес в hex (например, 1F0):"), QLineEdit::Normal,
        QString(), &ok);
    if (!ok || text.trimmed().isEmpty()) {
        return;
    }

    bool parsed = false;
    const uint offset = text.trimmed().toUInt(&parsed, 16);
    if (!parsed || !gotoOffset(offset)) {
        return;
    }
}

void HexEditor::requestFind()
{
    bool ok = false;
    const QString text = QInputDialog::getText(
        this, QStringLiteral("Поиск по дампу"),
        QStringLiteral("Байты в hex через пробел (например, AA BB CC):"),
        QLineEdit::Normal, QString(), &ok);
    if (!ok || text.trimmed().isEmpty()) {
        return;
    }

    bool parsed = false;
    const QByteArray needle = parseHexSequence(text, &parsed);
    if (!parsed || needle.isEmpty()) {
        return;
    }

    const QTextCursor cursor = textCursor();
    const int posInBlock = cursor.positionInBlock();
    int column = 0;
    if (posInBlock >= kHexOffset) {
        column = qBound(0, (posInBlock - kHexOffset) / kHexTokenWidth, kBytesPerLine - 1);
    }
    const quint32 from = static_cast<quint32>(qMax(0, cursor.blockNumber() * kBytesPerLine + column));

    if (!findBytes(needle, from) && from > 0) {
        findBytes(needle, 0);
    }
}

void HexEditor::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu *menu = createStandardContextMenu();
    menu->addSeparator();

    QAction *gotoAction = menu->addAction(QStringLiteral("Перейти к адресу… (Ctrl+G)"));
    connect(gotoAction, &QAction::triggered, this, &HexEditor::requestGoto);

    QAction *findAction = menu->addAction(QStringLiteral("Найти байты… (Ctrl+F)"));
    connect(findAction, &QAction::triggered, this, &HexEditor::requestFind);

    menu->addSeparator();
    QAction *fillFfAction = menu->addAction(QStringLiteral("Заполнить всё 0xFF"));
    connect(fillFfAction, &QAction::triggered, this, [this]() { fill(0xFF); });
    QAction *fill00Action = menu->addAction(QStringLiteral("Заполнить всё 0x00"));
    connect(fill00Action, &QAction::triggered, this, [this]() { fill(0x00); });

    menu->exec(event->globalPos());
    delete menu;
}
