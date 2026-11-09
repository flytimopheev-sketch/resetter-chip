#ifndef HEX_EDITOR_H
#define HEX_EDITOR_H

#include <QByteArray>
#include <QPlainTextEdit>

// HEX-редактор дампа.
// Формат строки: "00000000:  XX XX ... XX  |ascii|"
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

signals:
    void dataChanged(const QByteArray &data);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onTextChanged();

private:
    QByteArray m_originalData;
    QByteArray m_currentData;
    bool m_updating = false;

    void rebuildText();

    // Разобрать текст обратно в байты. Возвращает false, если формат нарушен.
    bool parseText(const QString &text, QByteArray &out) const;
};

#endif // HEX_EDITOR_H
