#include "NovaWaveformRenderer.h"
#include "NovaWaveformItem.h"
#include "core/NovaLogging.h"
#include <QOpenGLContext>
#include <QQuickFramebufferObject>
#include <QVector2D>
#include <cmath>
#include <algorithm>
#include <QOpenGLFunctions>
#include <QDebug>
#ifndef Q_OS_ANDROID
#include <GL/gl.h>
#else
#include <GLES3/gl3.h>
#endif

static const char *vertexShaderSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in float aType;
out float vType;
uniform vec2 uViewportSize;
uniform vec2 uViewport;
uniform float uPxPerBeat;
uniform float uGainDb;
uniform float uFadeIn;
uniform float uFadeOut;
void main() {
    vType = aType;
    vec2 ndc = vec2(aPos.x / max(uViewportSize.x, 1.0) * 2.0 - 1.0,
                    1.0 - aPos.y / max(uViewportSize.y, 1.0) * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
}
)";

static const char *fragmentShaderSrc = R"(
#version 330 core
in float vType;
out vec4 fragColor;
uniform vec4 uPeakColor;
uniform vec4 uRmsColor;
uniform vec4 uBgColor;
uniform vec4 uSelColor;
uniform bool uSelected;
void main() {
    vec4 color;
    float alpha = 1.0;
    if (vType < 0.5) {           // peak
        color = uPeakColor;
    } else if (vType < 1.5) {    // rms
        color = uRmsColor;
        alpha = 0.6;
    } else {                      // fade curve
        color = vec4(1.0, 1.0, 1.0, 0.3);
    }
    if (uSelected) {
        color = mix(color, uSelColor, 0.4);
    }
    fragColor = vec4(color.rgb, color.a * alpha);
}
)";

static const char *vertexShaderSrcES = R"(
#version 300 es
layout(location = 0) in vec2 aPos;
layout(location = 1) in float aType;
out float vType;
uniform vec2 uViewportSize;
void main() {
    vType = aType;
    vec2 ndc = vec2(aPos.x / max(uViewportSize.x, 1.0) * 2.0 - 1.0,
                    1.0 - aPos.y / max(uViewportSize.y, 1.0) * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
}
)";

static const char *fragmentShaderSrcES = R"(
#version 300 es
precision mediump float;
in float vType;
out vec4 fragColor;
uniform vec4 uPeakColor;
uniform vec4 uRmsColor;
uniform vec4 uBgColor;
uniform vec4 uSelColor;
uniform bool uSelected;
void main() {
    vec4 color;
    float alpha = 1.0;
    if (vType < 0.5) {
        color = uPeakColor;
    } else if (vType < 1.5) {
        color = uRmsColor;
        alpha = 0.6;
    } else {
        color = vec4(1.0, 1.0, 1.0, 0.3);
    }
    if (uSelected) {
        color = mix(color, uSelColor, 0.4);
    }
    fragColor = vec4(color.rgb, color.a * alpha);
}
)";

NovaWaveformRenderer::NovaWaveformRenderer() {
}

NovaWaveformRenderer::~NovaWaveformRenderer() {
    if (m_glFuncs) {
        if (m_vbo) m_glFuncs->glDeleteBuffers(1, &m_vbo);
        if (m_vao && glDeleteVertexArrays_) glDeleteVertexArrays_(1, &m_vao);
        if (m_ebo) m_glFuncs->glDeleteBuffers(1, &m_ebo);
    }
    delete m_program;
    delete m_fbo;
}

void NovaWaveformRenderer::initGL() {
    if (m_glInitialized) return;

    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    if (!ctx) {
        qWarning() << "No current OpenGL context";
        return;
    }

    m_glFuncs = ctx->functions();
    m_glFuncs->initializeOpenGLFunctions();

    // Resolve OpenGL 3.2+ functions using class member function pointers
    glGenVertexArrays_ = (PFNGLGENVERTEXARRAYSPROC)ctx->getProcAddress("glGenVertexArrays");
    glBindVertexArray_ = (PFNGLBINDVERTEXARRAYPROC)ctx->getProcAddress("glBindVertexArray");
    glGenBuffers_ = (PFNGLGENBUFFERSPROC)ctx->getProcAddress("glGenBuffers");
    glBindBuffer_ = (PFNGLBINDBUFFERPROC)ctx->getProcAddress("glBindBuffer");
    glBufferData_ = (PFNGLBUFFERDATAPROC)ctx->getProcAddress("glBufferData");
    glEnableVertexAttribArray_ = (PFNGLENABLEVERTEXATTRIBARRAYPROC)ctx->getProcAddress("glEnableVertexAttribArray");
    glVertexAttribPointer_ = (PFNGLVERTEXATTRIBPOINTERPROC)ctx->getProcAddress("glVertexAttribPointer");
    glDeleteVertexArrays_ = (PFNGLDELETEVERTEXARRAYSPROC)ctx->getProcAddress("glDeleteVertexArrays");
    glDeleteBuffers_ = (PFNGLDELETEBUFFERSPROC)ctx->getProcAddress("glDeleteBuffers");

    if (!glGenVertexArrays_ || !glBindVertexArray_) {
        qWarning() << "VAO functions not available";
        return;
    }

    m_program = new QOpenGLShaderProgram();
    const bool isES = ctx->isOpenGLES();
    const char *vs = isES ? vertexShaderSrcES : vertexShaderSrc;
    const char *fs = isES ? fragmentShaderSrcES : fragmentShaderSrc;
    if (!m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vs)) {
        qWarning() << "Waveform vertex shader error:" << m_program->log();
        delete m_program; m_program = nullptr; return;
    }
    if (!m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fs)) {
        qWarning() << "Waveform fragment shader error:" << m_program->log();
        delete m_program; m_program = nullptr; return;
    }
    if (!m_program->link()) {
        qWarning() << "Waveform shader link error:" << m_program->log();
        delete m_program; m_program = nullptr; return;
    }

    glGenVertexArrays_(1, &m_vao);
    glGenBuffers_(1, &m_vbo);
    glGenBuffers_(1, &m_ebo);

    m_glInitialized = true;
}

void NovaWaveformRenderer::rebuildGeometry() {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_vertices.clear();
    m_indices.clear();

    const auto &peaks = (m_pixelsPerBeat > 2.0) ? m_peaks : m_overview;

    int count = static_cast<int>(peaks.size());
    float gain = std::pow(10.0f, static_cast<float>(m_gainDb) / 20.0f);
    float centerY = m_height * 0.5f;
    float maxAmp = m_height * 0.44f;

    const double totalBeats = m_endBeat - m_startBeat;
    const double pxPerBeatSafe = (m_pixelsPerBeat > 0.0) ? m_pixelsPerBeat : 100.0;
    const double totalPx = totalBeats * pxPerBeatSafe;

    m_vertices.reserve(count * 4 * 3 + 12); // 4 vertices per peak (quad), 3 floats each
    m_indices.reserve(count * 6 + 6);

    int vertexIdx = 0;

    // ── LÍNEA DE SILENCIO (EJE) ──────────────────────────────────────────
    // Se dibuja siempre, aunque no haya picos: en los tramos sin sonido el
    // clip muestra una linea horizontal (como en FL Studio/Logic) en vez de
    // quedar completamente vacio.
    {
        const float lineH = 0.75f;
        const float x0 = 0.0f;
        const float x1 = static_cast<float>(totalPx);
        const int base = vertexIdx;
        m_vertices.push_back(x0); m_vertices.push_back(centerY - lineH); m_vertices.push_back(2.0f);
        m_vertices.push_back(x1); m_vertices.push_back(centerY - lineH); m_vertices.push_back(2.0f);
        m_vertices.push_back(x1); m_vertices.push_back(centerY + lineH); m_vertices.push_back(2.0f);
        m_vertices.push_back(x0); m_vertices.push_back(centerY + lineH); m_vertices.push_back(2.0f);
        m_indices.push_back(base);
        m_indices.push_back(base + 1);
        m_indices.push_back(base + 2);
        m_indices.push_back(base + 2);
        m_indices.push_back(base + 3);
        m_indices.push_back(base);
        vertexIdx += 4;
    }

    if (peaks.empty()) {
        m_dirtyGeometry = false;
        return;
    }

    const double peakWidthPx = (count > 0) ? std::max(1.0, totalPx / count) : 1.0;
    for (int i = 0; i < count; ++i) {
        float minVal = peaks[i].min;
        float maxVal = peaks[i].max;
        float rmsVal = peaks[i].rms;

        // Apply gain
        minVal *= gain;
        maxVal *= gain;
        rmsVal *= gain;

        // Clamp
        minVal = std::max(-1.0f, std::min(0.0f, minVal));
        maxVal = std::max(0.0f, std::min(1.0f, maxVal));
        rmsVal = std::max(0.0f, std::min(1.0f, rmsVal));

        float yTop = centerY - maxVal * maxAmp;
        float yBottom = centerY - minVal * maxAmp;
        float yRmsTop = centerY - rmsVal * maxAmp;
        float yRmsBottom = centerY + rmsVal * maxAmp;

        float x = static_cast<float>(i * peakWidthPx);
        const float w = static_cast<float>(peakWidthPx);

        // Peak quad (type 0)
        m_vertices.push_back(x); m_vertices.push_back(yTop); m_vertices.push_back(0.0f);
        m_vertices.push_back(x + w); m_vertices.push_back(yTop); m_vertices.push_back(0.0f);
        m_vertices.push_back(x + w); m_vertices.push_back(yBottom); m_vertices.push_back(0.0f);
        m_vertices.push_back(x); m_vertices.push_back(yBottom); m_vertices.push_back(0.0f);

        // RMS quad (type 1) - draw behind peak
        m_vertices.push_back(x); m_vertices.push_back(yRmsTop); m_vertices.push_back(1.0f);
        m_vertices.push_back(x + w); m_vertices.push_back(yRmsTop); m_vertices.push_back(1.0f);
        m_vertices.push_back(x + w); m_vertices.push_back(yRmsBottom); m_vertices.push_back(1.0f);
        m_vertices.push_back(x); m_vertices.push_back(yRmsBottom); m_vertices.push_back(1.0f);

        // Indices for two quads (peak + rms)
        for (int q = 0; q < 2; ++q) {
            int base = vertexIdx + q * 4;
            m_indices.push_back(base);
            m_indices.push_back(base + 1);
            m_indices.push_back(base + 2);
            m_indices.push_back(base + 2);
            m_indices.push_back(base + 3);
            m_indices.push_back(base);
        }
        vertexIdx += 8;
    }

    m_dirtyGeometry = false;
}

void NovaWaveformRenderer::uploadGeometry() {
    if (!m_glInitialized) initGL();
    if (!m_glInitialized) return;

    glBindVertexArray_(m_vao);
    glBindBuffer_(GL_ARRAY_BUFFER, m_vbo);
    glBufferData_(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(float), m_vertices.data(), GL_DYNAMIC_DRAW);

    glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData_(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof(GLuint), m_indices.data(), GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray_(0);
    glVertexAttribPointer_(0, 2, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray_(1);
    glVertexAttribPointer_(1, 1, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindVertexArray_(0);
}

void NovaWaveformRenderer::render() {
    if (!m_glInitialized) initGL();
    if (!m_glInitialized || !m_program) return;

    if (m_dirtyGeometry) {
        rebuildGeometry();
        uploadGeometry();
    }

    if (m_vertices.empty()) return;

    m_program->bind();
    m_program->setUniformValue("uViewportSize", QVector2D(m_width, m_height));
    m_program->setUniformValue("uViewport", QVector2D(m_startBeat, m_endBeat));
    m_program->setUniformValue("uPxPerBeat", static_cast<float>(m_pixelsPerBeat));
    m_program->setUniformValue("uGainDb", static_cast<float>(m_gainDb));
    m_program->setUniformValue("uFadeIn", static_cast<float>(m_fadeIn));
    m_program->setUniformValue("uFadeOut", static_cast<float>(m_fadeOut));

    m_program->setUniformValue("uPeakColor", m_peakColor);
    m_program->setUniformValue("uRmsColor", m_rmsColor);
    m_program->setUniformValue("uBgColor", m_bgColor);
    m_program->setUniformValue("uSelColor", m_selColor);
    m_program->setUniformValue("uSelected", m_selected);

    glBindVertexArray_(m_vao);
    m_glFuncs->glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_indices.size()), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray_(0);

    m_program->release();
}

QOpenGLFramebufferObject *NovaWaveformRenderer::createFramebufferObject(const QSize &size) {
    m_width = size.width();
    m_height = size.height();

    QOpenGLFramebufferObjectFormat format;
    format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    format.setSamples(4); // MSAA 4x
    format.setTextureTarget(GL_TEXTURE_2D);

    delete m_fbo;
    m_fbo = new QOpenGLFramebufferObject(size, format);

    m_dirtyGeometry = true;
    return m_fbo;
}

void NovaWaveformRenderer::synchronize(QQuickFramebufferObject *item) {
    // Called on GUI thread - safe to access item properties
    auto *wf = qobject_cast<NovaWaveformItem*>(item);
    if (!wf)
        return;
    std::lock_guard<std::mutex> lock(m_dataMutex);
    bool changed = false;
    if (m_peakColor != wf->peakColor()) { m_peakColor = wf->peakColor(); }
    if (m_rmsColor != wf->rmsColor()) { m_rmsColor = wf->rmsColor(); }
    if (m_selected != wf->isSelected()) { m_selected = wf->isSelected(); }
    if (!qFuzzyCompare(m_fadeIn, wf->fadeInBeats()) ||
        !qFuzzyCompare(m_fadeOut, wf->fadeOutBeats()) ||
        !qFuzzyCompare(m_gainDb, static_cast<double>(wf->clipGainDb()))) {
        m_fadeIn = wf->fadeInBeats();
        m_fadeOut = wf->fadeOutBeats();
        m_gainDb = wf->clipGainDb();
        changed = true;
    }
    const int w = static_cast<int>(wf->width());
    const int h = static_cast<int>(wf->height());
    if (w > 0 && h > 0 && (w != m_width || h != m_height)) {
        m_width = w;
        m_height = h;
        changed = true;
    }

    // ── Ventana temporal del clip: donde se dibujan los picos ──────────
    const double startBeat = wf->startBeat();
    const double endBeat = wf->startBeat() + wf->lengthBeats();
    const double pixelsPerBeat = wf->pixelsPerBeat();
    if (!qFuzzyCompare(m_startBeat, startBeat) ||
        !qFuzzyCompare(m_endBeat, endBeat) ||
        !qFuzzyCompare(m_pixelsPerBeat, pixelsPerBeat)) {
        m_startBeat = startBeat;
        m_endBeat = endBeat;
        m_pixelsPerBeat = pixelsPerBeat;
        changed = true;
    }

    // ── Picos de la region ──────────────────────────────────────────────
    // Antes el renderer no recibia nunca datos: setPeaks()/setViewport()
    // no los llamaba nadie y la forma de onda no se dibujaba. Ahora se
    // piden a la cache por regionId.
    const QString regionId = wf->regionId();
    const bool regionChanged = (regionId != m_lastRegionId);
    if (regionChanged) {
        m_lastRegionId = regionId;
        m_peaks.clear();
        m_overview.clear();
        changed = true;
    }
    // Los picos pueden llegar despues (se calculan al cargar el proyecto):
    // si aun no tenemos datos pero la cache los tiene, se recuperan.
    if (!regionId.isEmpty() && (regionChanged || m_peaks.empty())) {
        if (NovaWaveformCache::hasPeaks(regionId)) {
            PeakData data;
            if (NovaWaveformCache::getPeaks(regionId, data)) {
                m_peaks = data.peaks;
                m_overview = data.overview;
                changed = true;
                qCDebug(novaModel) << "🎨 Onda: recibidos" << m_peaks.size()
                                   << "picos para" << regionId.toStdString();
            }
        } else if (regionChanged) {
            qCDebug(novaModel) << "🎨 Onda: todavía sin picos para" << regionId.toStdString();
        }
    }

    if (changed)
        m_dirtyGeometry = true;
}

void NovaWaveformRenderer::setPeaks(const PeakData &data) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_peaks = data.peaks;
    m_overview = data.overview;
    m_dirtyGeometry = true;
}

void NovaWaveformRenderer::clearPeaks() {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_peaks.clear();
    m_overview.clear();
    m_vertices.clear();
    m_indices.clear();
    m_dirtyGeometry = false;
}

void NovaWaveformRenderer::setViewport(double startBeat, double endBeat, double pixelsPerBeat) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_startBeat = startBeat;
    m_endBeat = endBeat;
    m_pixelsPerBeat = pixelsPerBeat;
    m_dirtyGeometry = true;
}

void NovaWaveformRenderer::setColors(const QColor &peakColor, const QColor &rmsColor, const QColor &bgColor) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_peakColor = peakColor;
    m_rmsColor = rmsColor;
    m_bgColor = bgColor;
}

void NovaWaveformRenderer::setSelection(bool selected, const QColor &selColor) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_selected = selected;
    m_selColor = selColor;
}

void NovaWaveformRenderer::setFadeInOut(double fadeInBeats, double fadeOutBeats, double clipGainDb) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_fadeIn = fadeInBeats;
    m_fadeOut = fadeOutBeats;
    m_gainDb = clipGainDb;
    m_dirtyGeometry = true;
}

void NovaWaveformRenderer::setSize(int width, int height) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_width = width;
    m_height = height;
    m_dirtyGeometry = true;
}