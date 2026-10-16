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

#include "GpuScreenWidget.h"
#pragma comment(lib, "opengl32.lib")
#include <QPainter>
#include <QFont>
#include <QMutexLocker>
#include <QOpenGLContext>
#include <QMouseEvent>
#include <QOpenGLShaderProgram>
#include <QVector2D>
#include <QDebug>

GpuScreenWidget::GpuScreenWidget(QWidget* parent)
    : QOpenGLWidget(parent)
{
    setAutoFillBackground(false);
    setMouseTracking(true);
}

GpuScreenWidget::~GpuScreenWidget()
{
    makeCurrent();

    delete crtProgram;
    crtProgram = nullptr;

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

    buildCrtProgram();
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

    // CRT shader tự lo việc làm mượt theo chiều ngang (sharp-bilinear) nên cần
    // texture ở chế độ LINEAR; các chế độ còn lại giữ NEAREST (pixel-perfect).
    const bool useCrt = crtLiteEnabled && crtProgram != nullptr;
    const GLint texFilter = useCrt ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, texFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, texFilter);

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
    if (useCrt)
    {
        // Kích thước THẬT (pixel thiết bị) của vùng ảnh được vẽ, để shader tính
        // scanline/mask đúng kể cả khi màn hình HiDPI.
        GLint vp[4] = { 0, 0, 1, 1 };
        glGetIntegerv(GL_VIEWPORT, vp);

        crtProgram->bind();
        crtProgram->setUniformValue("tex", 0);
        crtProgram->setUniformValue("texSize", QVector2D(256.0f, 240.0f));
        crtProgram->setUniformValue("outSize", QVector2D(vp[2] * x, vp[3] * y));
        crtProgram->setUniformValue("curvature", crtSettings.curvature);
        crtProgram->setUniformValue("scanlineStrength", crtSettings.scanline);
        crtProgram->setUniformValue("maskStrength", crtSettings.mask);
        crtProgram->setUniformValue("glow", crtSettings.glow);
        crtProgram->setUniformValue("vignette", crtSettings.vignette);
        crtProgram->setUniformValue("brightness", crtSettings.brightness);
        crtProgram->setUniformValue("softness", crtSettings.softness);
    }

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

    if (useCrt)
        crtProgram->release();

    glDisable(GL_TEXTURE_2D);

    if (scanlineEnabled && !useCrt)
    {
        glDisable(GL_TEXTURE_2D);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Alpha càng thấp càng tự nhiên
        glColor4f(0.0f, 0.0f, 0.0f, 0.22f);

        glBegin(GL_QUADS);

        for (int line = 1; line < 240; line += 2)
        {
            // Mỗi scanline NES trong tọa độ OpenGL
            float lineTop = y - (2.0f * y * line / 240.0f);
            float lineBottom = y - (2.0f * y * (line + 1) / 240.0f);

            // Chỉ phủ khoảng 55% chiều cao dòng cho mềm hơn
            float center = (lineTop + lineBottom) * 0.5f;
            float halfHeight = (lineTop - lineBottom) * 0.28f;

            float y1 = center + halfHeight;
            float y2 = center - halfHeight;

            glVertex2f(-x, y1);
            glVertex2f(x, y1);
            glVertex2f(x, y2);
            glVertex2f(-x, y2);
        }

        glEnd();

        glDisable(GL_BLEND);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);


    }
    if (crtLiteEnabled && !useCrt)   // CRT lite cũ chỉ còn là fallback khi shader lỗi
    {
        glDisable(GL_TEXTURE_2D);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glShadeModel(GL_SMOOTH);

        // 1) Scanline rất nhẹ
        glColor4f(0.0f, 0.0f, 0.0f, 0.12f);

        glBegin(GL_QUADS);

        for (int line = 1; line < 240; line += 2)
        {
            float lineTop = y - (2.0f * y * line / 240.0f);
            float lineBottom = y - (2.0f * y * (line + 1) / 240.0f);

            float center = (lineTop + lineBottom) * 0.5f;
            float halfHeight = (lineTop - lineBottom) * 0.22f;

            float y1 = center + halfHeight;
            float y2 = center - halfHeight;

            glVertex2f(-x, y1);
            glVertex2f(x, y1);
            glVertex2f(x, y2);
            glVertex2f(-x, y2);
        }

        glEnd();

        // 2) Vignette mềm 4 cạnh, dùng gradient alpha
        float edgeW = x * 0.18f;
        float edgeH = y * 0.14f;

        glBegin(GL_QUADS);

        // Cạnh trái: ngoài tối -> trong trong suốt
        glColor4f(0.0f, 0.0f, 0.0f, 0.22f);
        glVertex2f(-x, y);
        glVertex2f(-x, -y);

        glColor4f(0.0f, 0.0f, 0.0f, 0.00f);
        glVertex2f(-x + edgeW, -y);
        glVertex2f(-x + edgeW, y);

        // Cạnh phải
        glColor4f(0.0f, 0.0f, 0.0f, 0.00f);
        glVertex2f(x - edgeW, y);
        glVertex2f(x - edgeW, -y);

        glColor4f(0.0f, 0.0f, 0.0f, 0.22f);
        glVertex2f(x, -y);
        glVertex2f(x, y);

        // Cạnh trên
        glColor4f(0.0f, 0.0f, 0.0f, 0.18f);
        glVertex2f(-x, y);
        glVertex2f(x, y);

        glColor4f(0.0f, 0.0f, 0.0f, 0.00f);
        glVertex2f(x, y - edgeH);
        glVertex2f(-x, y - edgeH);

        // Cạnh dưới
        glColor4f(0.0f, 0.0f, 0.0f, 0.00f);
        glVertex2f(-x, -y + edgeH);
        glVertex2f(x, -y + edgeH);

        glColor4f(0.0f, 0.0f, 0.0f, 0.18f);
        glVertex2f(x, -y);
        glVertex2f(-x, -y);

        glEnd();

        // 3) Phủ tối toàn màn cực nhẹ để dịu màu
        glColor4f(0.0f, 0.0f, 0.0f, 0.04f);

        glBegin(GL_QUADS);
        glVertex2f(-x, y);
        glVertex2f(x, y);
        glVertex2f(x, -y);
        glVertex2f(-x, -y);
        glEnd();

        glDisable(GL_BLEND);

        glShadeModel(GL_FLAT);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    }

    if (!overlayText.isEmpty())
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::TextAntialiasing, true);

        QFont font("Consolas", 11, QFont::Bold);
        painter.setFont(font);

        // Nền đen mờ phía sau chữ
        QRect textRect = painter.fontMetrics().boundingRect(
            QRect(0, 0, 10000, 10000), Qt::AlignLeft | Qt::AlignTop, overlayText);
        QRect box(8, 8, textRect.width() + 12, textRect.height() + 8);
        painter.fillRect(box, QColor(0, 0, 0, 150));

        painter.setPen(QColor(255, 255, 0));
        painter.drawText(
            QRect(14, 12, width() - 28, height() - 24),
            Qt::AlignLeft | Qt::AlignTop,
            overlayText
        );
    }
}

void GpuScreenWidget::setScanlineEnabled(bool enabled)
{
    scanlineEnabled = enabled;
    update();
}

void GpuScreenWidget::setCrtLiteEnabled(bool enabled)
{
    crtLiteEnabled = enabled;
    update();
}
void GpuScreenWidget::setOverlayText(const QString& text)
{
    overlayText = text;
    update();
}
QPoint GpuScreenWidget::mapToNes(const QPoint& widgetPos) const
{
    float widgetAspect = width() / float(height());
    float nesAspect = 256.0f / 240.0f;

    float x = 1.0f, y = 1.0f;
    if (widgetAspect > nesAspect) x = nesAspect / widgetAspect;
    else                          y = widgetAspect / nesAspect;

    float drawnW = width() * x;
    float drawnH = height() * y;
    float offsetX = (width() - drawnW) / 2.0f;
    float offsetY = (height() - drawnH) / 2.0f;

    float relX = (widgetPos.x() - offsetX) / drawnW;
    float relY = (widgetPos.y() - offsetY) / drawnH;

    int nesX = int(relX * 256.0f);
    int nesY = int(relY * 240.0f);

    if (nesX < 0 || nesX >= 256 || nesY < 0 || nesY >= 240)
        return QPoint(-1, -1);

    return QPoint(nesX, nesY);
}

void GpuScreenWidget::mouseMoveEvent(QMouseEvent* event)
{
    QPoint nes = mapToNes(event->pos());
    emit zapperPositionChanged(nes.x(), nes.y());
    QOpenGLWidget::mouseMoveEvent(event);
}

void GpuScreenWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        emit zapperTriggerChanged(true);
    QOpenGLWidget::mousePressEvent(event);
}

void GpuScreenWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        emit zapperTriggerChanged(false);
    QOpenGLWidget::mouseReleaseEvent(event);
}
void GpuScreenWidget::setCrtSettings(const CrtSettings& settings)
{
    crtSettings = settings;
    update();
}

// ---------------------------------------------------------------------------
// CRT shader (GLSL 1.20 - chạy được trên compatibility profile, cùng kiểu với
// code fixed-pipeline glBegin/glEnd ở trên).
//
//  1. Cong màn hình (barrel distortion) + bo góc có khử răng cưa
//  2. Sample "sharp-bilinear" theo chiều ngang: pixel NES vẫn sắc nét nhưng
//     không bị răng cưa khi phóng to không chẵn
//  3. Scanline dạng Gaussian, khe scanline hẹp lại ở chỗ sáng (đúng kiểu tia
//     điện tử: điểm sáng thì "phình" ra), tính trong không gian ánh sáng tuyến tính
//  4. Bloom/halation nhẹ (lấy mẫu 4 điểm lân cận)
//  5. Mặt nạ phosphor kiểu aperture grille (sọc RGB), đã chuẩn hoá để không
//     làm tối trung bình
//  6. Vignette + bù sáng
// Scanline/mask tự mờ dần khi cửa sổ nhỏ (< 2x độ phân giải NES) để tránh moiré.
// ---------------------------------------------------------------------------
static const char* kCrtVertexShader = R"GLSL(#version 120
void main()
{
    gl_Position = gl_Vertex;
    gl_TexCoord[0] = gl_MultiTexCoord0;
}
)GLSL";

static const char* kCrtFragmentShader = R"GLSL(#version 120
uniform sampler2D tex;
uniform vec2  texSize;
uniform vec2  outSize;
uniform float curvature;
uniform float scanlineStrength;
uniform float maskStrength;
uniform float glow;
uniform float vignette;
uniform float brightness;
uniform float softness;

vec3 toLinear(vec3 c) { return pow(c, vec3(2.2)); }
vec3 toGamma(vec3 c)  { return pow(c, vec3(1.0 / 2.2)); }

void main()
{
    vec2 uv = gl_TexCoord[0].xy;

    // 1) Cong man hinh
    vec2 c = uv * 2.0 - 1.0;
    c *= 1.0 + curvature * (c.yx * c.yx);
    vec2 w = c * 0.5 + 0.5;

    float edge = 1.0;
    if (curvature > 0.0)
    {
        vec2 edgePx = min(w, 1.0 - w) * outSize;
        edge = clamp(min(edgePx.x, edgePx.y) / 1.5, 0.0, 1.0);
        if (edge <= 0.0)
        {
            gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
            return;
        }
    }

    // 2) Sharp-bilinear ngang, giu nguyen dong doc (scanline tu xu ly)
    vec2 p = w * texSize;
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 scale = max(outSize / texSize, vec2(1.0));

    // softness cao -> bo vung "pixel sac", noi suy muot nhu tia dien tu
    float scaleX = mix(scale.x, 1.0, softness);
    float rangeX = 0.5 - 0.5 / scaleX;
    float dx = f.x - 0.5;
    float fx = (dx - clamp(dx, -rangeX, rangeX)) * scaleX + 0.5;
    vec2 sp = vec2(i.x + fx, i.y + 0.5) / texSize;

    // Nhoe ngang nhe (giong tin hieu analog): pixel ranh gioi hoa vao nhau
    float ox = 0.75 / texSize.x;
    float wt = 0.25 * softness;
    vec3 col = toLinear(texture2D(tex, sp).rgb) * (1.0 - 2.0 * wt)
             + (toLinear(texture2D(tex, sp + vec2(ox, 0.0)).rgb)
              + toLinear(texture2D(tex, sp - vec2(ox, 0.0)).rgb)) * wt;

    // 4) Bloom nhe: trung binh 4 diem lan can
    vec2 o = 1.5 / texSize;
    vec3 halo = toLinear(texture2D(tex, sp + vec2( o.x, 0.0)).rgb)
              + toLinear(texture2D(tex, sp - vec2( o.x, 0.0)).rgb)
              + toLinear(texture2D(tex, sp + vec2(0.0,  o.y)).rgb)
              + toLinear(texture2D(tex, sp - vec2(0.0,  o.y)).rgb);
    halo *= 0.25;

    // 3) Scanline Gaussian, cang sang cang "phinh"
    float fade = clamp((scale.y - 1.0) / 0.75, 0.0, 1.0);
    float d = f.y - 0.5;
    vec3 sigma = mix(vec3(0.16), vec3(0.34), clamp(col, 0.0, 1.0));
    vec3 beam = exp(-(d * d) / (2.0 * sigma * sigma));
    col *= mix(vec3(1.0), beam, scanlineStrength * fade);

    col += halo * glow * 0.30;

    // 5) Mat na phosphor (aperture grille), chuan hoa trung binh = 1
    float m = mod(floor(gl_FragCoord.x), 3.0);
    vec3 k = vec3(0.25);
    if (m < 0.5)      k.r = 1.0;
    else if (m < 1.5) k.g = 1.0;
    else              k.b = 1.0;
    k *= 2.0;
    col *= mix(vec3(1.0), k, maskStrength * fade);

    // 6) Vignette + bu sang
    col *= brightness;
    col *= 1.0 - vignette * 0.5 * dot(c, c);
    col *= edge;

    gl_FragColor = vec4(toGamma(clamp(col, 0.0, 1.0)), 1.0);
}
)GLSL";

bool GpuScreenWidget::buildCrtProgram()
{
    delete crtProgram;
    crtProgram = new QOpenGLShaderProgram(this);

    const bool ok =
        crtProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, kCrtVertexShader) &&
        crtProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, kCrtFragmentShader) &&
        crtProgram->link();

    if (!ok)
    {
        qWarning().noquote() << "CRT shader loi, dung CRT lite thay the:\n" << crtProgram->log();
        delete crtProgram;
        crtProgram = nullptr;
        return false;
    }
    return true;
}