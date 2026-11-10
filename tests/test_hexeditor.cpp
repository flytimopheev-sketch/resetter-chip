#include <QSignalSpy>
#include <QTest>

#include "gui/HexEditor.h"

// Тест проверяет реальный цикл: байты -> текст -> правка -> обратно байты
class TestHexEditor : public QObject
{
    Q_OBJECT

private slots:
    void rendersBytes()
    {
        HexEditor editor;
        const QByteArray data("\x01\x02\xAB\xCD", 4);
        editor.setDump(data);

        QCOMPARE(editor.dump(), data);
        const QString text = editor.toPlainText();
        QVERIFY(text.contains(QStringLiteral("01 02 AB CD")));
        QVERIFY(text.contains(QStringLiteral("00000000")));
    }

    void editingUpdatesDump()
    {
        HexEditor editor;
        const QByteArray data(16, '\x00');
        editor.setDump(data);

        QSignalSpy spy(&editor, &HexEditor::dataChanged);

        QString text = editor.toPlainText();
        text.replace(QStringLiteral("00 00 00"), QStringLiteral("7F 00 00"));
        editor.setPlainText(text);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(editor.dump().at(0), static_cast<char>(0x7F));
        QCOMPARE(editor.dump().size(), 16);
    }

    void wrongSizeIgnored()
    {
        HexEditor editor;
        const QByteArray data(8, '\x11');
        editor.setDump(data);

        // Пользователь пытается изменить размер дампа — правка игнорируется
        editor.setPlainText(QStringLiteral("00000000:  AA BB |..|\n"));
        QCOMPARE(editor.dump(), data);
    }

    void fillChangesAllBytes()
    {
        HexEditor editor;
        const QByteArray data(32, '\x00');
        editor.setDump(data);
        QSignalSpy spy(&editor, &HexEditor::dataChanged);

        editor.fill(0xFF);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(editor.dump(), QByteArray(32, '\xFF'));
    }

    void gotoAndFind()
    {
        HexEditor editor;
        QByteArray data(64, '\x00');
        data[0x2A] = '\x12';
        data[0x2B] = '\x34';
        editor.setDump(data);

        QVERIFY(editor.gotoOffset(0x2A));
        QVERIFY(editor.findBytes(QByteArray("\x12\x34", 2)));
        QVERIFY(!editor.findBytes(QByteArray("\xDE\xAD", 2)));
        QVERIFY(!editor.gotoOffset(0x1000));
    }
};

QTEST_MAIN(TestHexEditor)
#include "test_hexeditor.moc"
