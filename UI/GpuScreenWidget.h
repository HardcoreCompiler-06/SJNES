/*
 * SJNES - Nintendo Entertainment System Emulator
 * Copyright (C) 2026 Chiến
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QImage>
#include <QMutex>
#include <QString>
class QMouseEvent;
class QOpenGLShaderProgram;

class GpuScreenWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit GpuScreenWidget(QWidget* parent = nullptr);
    ~GpuScreenWidget();

    void setFrame(const QImage& image);
    void setScanlineEnabled(bool enabled);
    void setCrtLiteEnabled(bool enabled);
    void setOverlayText(const QString& text);

    struct CrtSettings {
        float curvature = 0.00f;
        float scanline = 0.90f;
        float mask = 0.35f;
        float glow = 0.55f;
        float vignette = 0.45f;
        float brightness = 1.25f;
        float softness = 0.85f;
    };
    void setCrtSettings(const CrtSettings& settings);
    bool crtShaderAvailable() const { return crtProgram != nullptr; }

signals:
    void zapperPositionChanged(int nesX, int nesY);
    void zapperTriggerChanged(bool pressed);


protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
private:
    QImage frame;
    QMutex frameMutex;

    GLuint textureId = 0;
    bool textureCreated = false;

    bool scanlineEnabled = false;
    bool crtLiteEnabled = false;

    QString overlayText;

    QOpenGLShaderProgram* crtProgram = nullptr;
    CrtSettings crtSettings;
    bool buildCrtProgram();

    QPoint mapToNes(const QPoint& widgetPos) const;
};