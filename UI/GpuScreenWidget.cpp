#include "GpuScreenWidget.h"
#pragma comment(lib, "opengl32.lib")

#include <QMutexLocker>
#include <QOpenGLContext>

GpuScreenWidget::GpuScreenWidget(QWidget* parent)
    : QOpenGLWidget(parent)
{
    setAutoFillBackground(false);
}

GpuScreenWidget::~GpuScreenWidget()
{
    makeCurrent();

    if (textureCreated) {
        glDeleteTextures(1, &textureId);
        textureCreated = false;
    }

    doneCurrent();
}

void GpuScreenWidget::initializeGL()
{
    initializeOpenGLFunctions();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);

    // Pixel-perfect, không làm mờ ảnh NES
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Tạo texture trống 256x240
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        256,
        240,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        nullptr
    );

    textureCreated = true;
}

void GpuScreenWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

void GpuScreenWidget::setFrame(const QImage& image)
{
    {
        QMutexLocker lock(&frameMutex);

        // OpenGL thích RGBA8888 hơn
        frame = image.convertToFormat(QImage::Format_RGBA8888);
    }

    update();
}

void GpuScreenWidget::paintGL()
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    QImage img;
    {
        QMutexLocker lock(&frameMutex);
        img = frame;
    }

    if (img.isNull()) {
        return;
    }

    glBindTexture(GL_TEXTURE_2D, textureId);

    // Upload framebuffer NES 256x240 lên GPU
    glTexSubImage2D(
        GL_TEXTURE_2D,
        0,
        0,
        0,
        256,
        240,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        img.constBits()
    );

    glEnable(GL_TEXTURE_2D);

    // Giữ đúng tỉ lệ 256:240
    float widgetAspect = width() / float(height());
    float nesAspect = 256.0f / 240.0f;

    float x = 1.0f;
    float y = 1.0f;

    if (widgetAspect > nesAspect) {
        x = nesAspect / widgetAspect;
    }
    else {
        y = widgetAspect / nesAspect;
    }

    // Vẽ quad bằng OpenGL fixed pipeline
    glBegin(GL_QUADS);

    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(-x, y);

    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(x, y);

    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(x, -y);

    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(-x, -y);

    glEnd();

    glDisable(GL_TEXTURE_2D);
}