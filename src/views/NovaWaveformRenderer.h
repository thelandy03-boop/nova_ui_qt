#ifndef NOVA_WAVEFORM_RENDERER_H
#define NOVA_WAVEFORM_RENDERER_H

#include <QQuickFramebufferObject>
#include <QOpenGLShaderProgram>
#include <QOpenGLFramebufferObject>
#include <QColor>
#include <vector>
#include <mutex>
#include <QOpenGLFunctions>
#include <QOpenGLContext>
#include <QDebug>
#ifndef Q_OS_ANDROID
#include <GL/gl.h>
#else
#include <GLES3/gl3.h>
#endif
#include "../models/NovaRegionModel.h"

class NovaWaveformRenderer : public QQuickFramebufferObject::Renderer {
public:
    NovaWaveformRenderer();
    ~NovaWaveformRenderer() override;
    // Liberar los recursos de OpenGL con el contexto todavia vivo.
    // Lo llama el item desde QQuickFramebufferObject::releaseResources().
    void releaseGLResources();

    void render() override;
    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override;
    void synchronize(QQuickFramebufferObject *item) override;

    void setPeaks(const PeakData &data);
    void clearPeaks();
    void setViewport(double startBeat, double endBeat, double pixelsPerBeat);
    void setColors(const QColor &peakColor, const QColor &rmsColor, const QColor &bgColor);
    void setSelection(bool selected, const QColor &selColor);
    void setFadeInOut(double fadeInBeats, double fadeOutBeats, double clipGainDb);
    void setSize(int width, int height);

private:
    void initGL();
    void rebuildGeometry();
    void uploadGeometry();

    QOpenGLShaderProgram *m_program = nullptr;
    QOpenGLFramebufferObject *m_fbo = nullptr;
    QOpenGLFunctions *m_glFuncs = nullptr;
    bool m_glInitialized = false;

    // OpenGL 3.2 Core function pointers
    PFNGLGENVERTEXARRAYSPROC glGenVertexArrays_ = nullptr;
    PFNGLBINDVERTEXARRAYPROC glBindVertexArray_ = nullptr;
    PFNGLGENBUFFERSPROC glGenBuffers_ = nullptr;
    PFNGLBINDBUFFERPROC glBindBuffer_ = nullptr;
    PFNGLBUFFERDATAPROC glBufferData_ = nullptr;
    PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray_ = nullptr;
    PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer_ = nullptr;
    PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays_ = nullptr;
    PFNGLDELETEBUFFERSPROC glDeleteBuffers_ = nullptr;

    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_ebo = 0;

    std::vector<float> m_vertices;
    std::vector<GLuint> m_indices;

    PeakData m_peakData;
    QColor m_peakColor = Qt::white;
    QColor m_rmsColor = QColor(255, 255, 255, 128);
    QColor m_bgColor = Qt::transparent;
    QColor m_selColor = QColor(255, 200, 0, 200);
    bool m_selected = false;
    double m_fadeIn = 0.0;
    double m_fadeOut = 0.0;
    double m_gainDb = 0.0;

    bool m_dirtyGeometry = true;
    double m_startBeat = 0.0;
    double m_endBeat = 1.0;
    double m_pixelsPerBeat = 100.0;
    int m_width = 640;
    int m_height = 120;

    std::vector<PeakPoint> m_peaks;
    std::vector<PeakPoint> m_overview;
    QString m_lastRegionId;
    quint64 m_lastPeaksGeneration = 0;

    std::mutex m_dataMutex;
};
#endif // NOVA_WAVEFORM_RENDERER_H