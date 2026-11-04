#ifndef NOVA_JSFX_HIGHLIGHTER_H
#define NOVA_JSFX_HIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QQuickTextDocument>
#include <QRegularExpression>
#include <QTextCharFormat>
#include <QVector>

/**
 * @brief Resaltador de sintaxis en tiempo real para scripts JSFX / EEL2 en QML.
 * 
 * Se conecta nativamente al QQuickTextDocument del TextEdit, ofreciendo rendimiento
 * de 60 FPS sin alterar el texto plano, el historial de deshacer ni el cursor.
 * 
 * Resalta secciones (@init, @sample, etc.), palabras clave, variables del motor de audio
 * (spl0, srate...), funciones matemáticas, números, cadenas y comentarios.
 */
class NovaJsfxHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT
    Q_PROPERTY(QQuickTextDocument* textDocument READ textDocument WRITE setTextDocument NOTIFY textDocumentChanged)

public:
    explicit NovaJsfxHighlighter(QObject *parent = nullptr);

    QQuickTextDocument* textDocument() const { return m_quickDoc; }
    void setTextDocument(QQuickTextDocument *doc);

signals:
    void textDocumentChanged();

protected:
    void highlightBlock(const QString &text) override;

private:
    void setupRules();

    struct HighlightingRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QVector<HighlightingRule> m_rules;

    QQuickTextDocument *m_quickDoc{nullptr};
};

#endif // NOVA_JSFX_HIGHLIGHTER_H
