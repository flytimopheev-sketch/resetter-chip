#ifndef HEX_EDITOR_H
#define HEX_EDITOR_H

#include <QByteArray>
#include <QPlainTextEdit>

// HEX-редактор дампа.
// Формат строки: "00000000:  XX XX ... XX |ascii|"
// Изменённые относительно оригинала байты показываются строчными буквами.
// Правки пользователя разбираются обратно в байты и отдаются сигналом dataChanged.
class HexEditor : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit HexEditor(QWidget *parent = nullptr);

    void setDump(const QByteArray &data);
    QByteArray dump() const;
    void clearDump();

    void highlightChanges(const QByteArray &original);

    // Перейти к байту и выделить его hex-представление
    bool gotoOffset(quint32 offset);

    // Найти последовательность байт; поиск идёт с начала или с fromOffset
    bool findBytes(const QByteArray &needle, quint32 fromOffset = 0);

    // Заполнить весь дамп одним значением
    void fill(quint8 value);

signals:
    void dataChanged(const QByteArray &data);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private slots:
    void onTextChanged();

private:
    QByteArray m_originalData;
    QByteArray m_currentData;
    bool m_updating = false;

    QString buildText() const;
    void rebuildText();

    // Разобрать текст обратно в байты. Возвращает false, если формат нарушен.
    bool parseText(const QString &text, QByteArray &out) const;

    void requestGoto();
    void requestFind();
};

#endif // HEX_EDITOR_H
