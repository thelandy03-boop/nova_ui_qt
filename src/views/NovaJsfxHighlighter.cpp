#include "NovaJsfxHighlighter.h"
#include <QColor>
#include <QFont>

NovaJsfxHighlighter::NovaJsfxHighlighter(QObject *parent)
    : QSyntaxHighlighter(parent)
{
    setupRules();
}

void NovaJsfxHighlighter::setTextDocument(QQuickTextDocument *doc)
{
    if (m_quickDoc == doc) {
        return;
    }
    m_quickDoc = doc;
    if (m_quickDoc) {
        setDocument(m_quickDoc->textDocument());
    } else {
        setDocument(nullptr);
    }
    emit textDocumentChanged();
}

void NovaJsfxHighlighter::setupRules()
{
    HighlightingRule rule;

    // 1. Números (enteros, flotantes y notación hexadecimal $x..)
    QTextCharFormat numberFormat;
    numberFormat.setForeground(QColor("#B5CEA8")); // Verde menta suave
    rule.pattern = QRegularExpression(QStringLiteral("\\b[0-9]+(\\.[0-9]+)?\\b|\\$x[0-9a-fA-F]+"));
    rule.format = numberFormat;
    m_rules.append(rule);

    // 2. Operadores matemáticos y lógicos
    QTextCharFormat operatorFormat;
    operatorFormat.setForeground(QColor("#569CD6")); // Azul acento
    rule.pattern = QRegularExpression(QStringLiteral("[=+\\-*/%<>&|^?!~:]"));
    rule.format = operatorFormat;
    m_rules.append(rule);

    // 3. Funciones matemáticas y utilidades integradas de EEL2
    QTextCharFormat functionFormat;
    functionFormat.setForeground(QColor("#DCDCAA")); // Amarillo dorado
    rule.pattern = QRegularExpression(QStringLiteral("\\b(sin|cos|tan|asin|acos|atan|atan2|sqr|sqrt|pow|exp|log|log10|abs|min|max|sign|rand|floor|ceil|invsqrt|memcpy|memset)\\b"));
    rule.format = functionFormat;
    m_rules.append(rule);

    // 4. Palabras clave de control de flujo
    QTextCharFormat keywordFormat;
    keywordFormat.setForeground(QColor("#C678DD")); // Magenta / Púrpura brillante
    keywordFormat.setFontWeight(QFont::Bold);
    rule.pattern = QRegularExpression(QStringLiteral("\\b(while|loop|if|else|match|function)\\b"));
    rule.format = keywordFormat;
    m_rules.append(rule);

    // 5. Variables de canal y parámetros de sliders (slider1, slider2...)
    QTextCharFormat sliderFormat;
    sliderFormat.setForeground(QColor("#4FC1FF")); // Celeste claro
    rule.pattern = QRegularExpression(QStringLiteral("\\bslider[0-9]+\\b"));
    rule.format = sliderFormat;
    m_rules.append(rule);

    // 6. Variables críticas del motor de audio (spl0, spl1, srate, tempo...)
    QTextCharFormat builtinVarFormat;
    builtinVarFormat.setForeground(QColor("#FF9E64")); // Naranja ámbar
    builtinVarFormat.setFontWeight(QFont::Bold);
    rule.pattern = QRegularExpression(QStringLiteral("\\b(spl[0-9]+|srate|num_ch|tempo|ts_num|ts_denom|play_state|play_position)\\b"));
    rule.format = builtinVarFormat;
    m_rules.append(rule);

    // 7. Encabezados de directivas JSFX (desc:, sliderN:..., in_pin:, out_pin:)
    QTextCharFormat headerFormat;
    headerFormat.setForeground(QColor("#4EC9B0")); // Turquesa
    headerFormat.setFontWeight(QFont::Bold);
    rule.pattern = QRegularExpression(QStringLiteral("(^\\s*|\\b)(desc|slider[0-9]+|in_pin|out_pin|filename):"));
    rule.format = headerFormat;
    m_rules.append(rule);

    // 8. Secciones del pipeline DSP (@init, @sample, @slider, @block, @gfx, @serialize)
    QTextCharFormat sectionFormat;
    sectionFormat.setForeground(QColor("#E5C07B")); // Oro / Púrpura destacado
    sectionFormat.setFontWeight(QFont::Bold);
    rule.pattern = QRegularExpression(QStringLiteral("@[a-zA-Z0-9_]+"));
    rule.format = sectionFormat;
    m_rules.append(rule);

    // 9. Cadenas de texto entre comillas dobles o simples
    QTextCharFormat stringFormat;
    stringFormat.setForeground(QColor("#CE9178")); // Salmón cálido
    rule.pattern = QRegularExpression(QStringLiteral("\".*?\"|'.*?'"));
    rule.format = stringFormat;
    m_rules.append(rule);

    // 10. Comentarios de línea única // (se aplican al final para sobreescribir tokens internos)
    QTextCharFormat commentFormat;
    commentFormat.setForeground(QColor("#6A9955")); // Verde oliva de comentarios
    commentFormat.setFontItalic(true);
    rule.pattern = QRegularExpression(QStringLiteral("//[^\n]*"));
    rule.format = commentFormat;
    m_rules.append(rule);
}

void NovaJsfxHighlighter::highlightBlock(const QString &text)
{
    // Aplicamos cada regla en orden de precedencia
    for (const HighlightingRule &rule : m_rules) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
}
