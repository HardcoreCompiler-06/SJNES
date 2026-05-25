#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QImage>
#include <QMutex>

class GpuScreenWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit GpuScreenWidget(QWidget* parent = nullptr);
    ~GpuScreenWidget();

    void setFrame(const QImage& image);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    GLuint textureId = 0;
    QImage frame;
    QMutex frameMutex;
    bool textureCreated = false;
};