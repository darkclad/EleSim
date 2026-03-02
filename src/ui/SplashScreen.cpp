#include "SplashScreen.h"

#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QScreen>
#include <QGuiApplication>
#include <QApplication>
#include <cmath>

static constexpr int SPLASH_W = 640;
static constexpr int SPLASH_H = 400;

// Easing: 0→1 over [startMs, endMs]
static qreal fadeIn(qreal t, qreal startMs, qreal endMs)
{
    if (t < startMs) return 0.0;
    if (t > endMs) return 1.0;
    return (t - startMs) / (endMs - startMs);
}

// Smooth ease-out
static qreal easeOut(qreal x)
{
    return 1.0 - (1.0 - x) * (1.0 - x);
}

SplashScreen::SplashScreen(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::SplashScreen);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setFixedSize(SPLASH_W, SPLASH_H);

    // Center on primary screen
    if (auto* screen = QGuiApplication::primaryScreen()) {
        QRect screenGeom = screen->availableGeometry();
        move(screenGeom.center() - QPoint(SPLASH_W / 2, SPLASH_H / 2));
    }

    m_elapsed.start();

    // 60fps repaint timer
    m_repaintTimer = new QTimer(this);
    connect(m_repaintTimer, &QTimer::timeout, this, QOverload<>::of(&QWidget::update));
    m_repaintTimer->start(16);
}

void SplashScreen::finish(QWidget* mainWin)
{
    m_mainWin = mainWin;

    // Wait until loading bar completes (~3.3s total), then fade out
    qreal elapsed = m_elapsed.elapsed();
    int remaining = qMax(0, 3300 - static_cast<int>(elapsed));

    QTimer::singleShot(remaining, this, [this]() {
        m_repaintTimer->stop();

        auto* fadeOut = new QPropertyAnimation(this, "opacity", this);
        fadeOut->setDuration(500);
        fadeOut->setStartValue(1.0);
        fadeOut->setEndValue(0.0);
        connect(fadeOut, &QPropertyAnimation::finished, this, [this]() {
            close();
            deleteLater();
        });
        fadeOut->start(QAbstractAnimation::DeleteWhenStopped);
    });
}

void SplashScreen::paintEvent(QPaintEvent*)
{
    qreal t = m_elapsed.elapsed();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    drawBackground(p);
    drawGrid(p, t);
    drawScanLine(p, t);
    drawCircuitTraces(p, t);
    drawNodeDots(p, t);
    drawIcon(p, t);
    drawTitle(p, t);
    drawTagline(p, t);
    drawLoadingBar(p, t);
    drawVersion(p, t);
}

// 1. Background: radial gradient
void SplashScreen::drawBackground(QPainter& p)
{
    QRadialGradient grad(QPointF(SPLASH_W * 0.5, SPLASH_H * 0.4), SPLASH_W * 0.7);
    grad.setColorAt(0.0, QColor(13, 34, 64));   // #0D2240
    grad.setColorAt(1.0, QColor(6, 14, 26));     // #060E1A
    p.fillRect(rect(), grad);
}

// 2. Grid: thin cyan lines, pulsing opacity
void SplashScreen::drawGrid(QPainter& p, qreal t)
{
    qreal pulse = 0.3 + 0.2 * std::sin(t / 2000.0 * M_PI);
    QColor gridColor(0, 200, 255, static_cast<int>(255 * 0.04 * pulse / 0.5));

    p.setPen(QPen(gridColor, 1));
    for (int x = 0; x < SPLASH_W; x += 48) {
        p.drawLine(x, 0, x, SPLASH_H);
    }
    for (int y = 0; y < SPLASH_H; y += 48) {
        p.drawLine(0, y, SPLASH_W, y);
    }
}

// 3. Scan line: sweeps top to bottom
void SplashScreen::drawScanLine(QPainter& p, qreal t)
{
    if (t < 1000) return;
    qreal cycle = std::fmod((t - 1000) / 4000.0, 1.0);
    int y = static_cast<int>(cycle * SPLASH_H);

    QLinearGradient grad(0, y, SPLASH_W, y);
    grad.setColorAt(0.0, Qt::transparent);
    grad.setColorAt(0.5, QColor(0, 229, 255, 20));
    grad.setColorAt(1.0, Qt::transparent);
    p.fillRect(0, y, SPLASH_W, 2, grad);
}

// 4. Circuit traces: 4 paths with animated dash offset
void SplashScreen::drawCircuitTraces(QPainter& p, qreal t)
{
    struct TraceInfo {
        QPointF offset;
        qreal scale;
        qreal delayMs;
        std::function<QPainterPath()> pathFn;
    };

    // Resistor zigzag trace
    auto resistorPath = []() {
        QPainterPath path;
        path.moveTo(0, 40); path.lineTo(40, 40); path.lineTo(40, 20);
        path.lineTo(80, 20); path.lineTo(80, 40); path.lineTo(100, 40);
        path.lineTo(110, 25); path.lineTo(120, 55); path.lineTo(130, 25);
        path.lineTo(140, 55); path.lineTo(150, 40); path.lineTo(200, 40);
        return path;
    };

    // Capacitor trace
    auto capacitorPath = []() {
        QPainterPath path;
        path.moveTo(0, 40); path.lineTo(50, 40); path.lineTo(50, 60);
        path.lineTo(90, 60); path.lineTo(90, 40); path.lineTo(130, 40);
        path.moveTo(130, 25); path.lineTo(130, 55);
        path.moveTo(145, 25); path.lineTo(145, 55);
        path.moveTo(145, 40); path.lineTo(200, 40);
        return path;
    };

    // Inductor trace
    auto inductorPath = []() {
        QPainterPath path;
        path.moveTo(0, 40); path.lineTo(60, 40);
        path.lineTo(70, 20); path.lineTo(80, 60); path.lineTo(90, 20);
        path.lineTo(100, 60); path.lineTo(110, 40);
        path.lineTo(160, 40); path.lineTo(160, 20); path.lineTo(200, 20);
        return path;
    };

    // Step trace
    auto stepPath = []() {
        QPainterPath path;
        path.moveTo(0, 40); path.lineTo(30, 40); path.lineTo(30, 15);
        path.lineTo(70, 15); path.lineTo(70, 40); path.lineTo(100, 40);
        path.lineTo(100, 65); path.lineTo(140, 65); path.lineTo(140, 40);
        path.lineTo(200, 40);
        return path;
    };

    TraceInfo traces[] = {
        { QPointF(32, 60),   0.8, 300,  resistorPath  },
        { QPointF(430, 280), 0.8, 600,  capacitorPath },
        { QPointF(50, 310),  0.8, 900,  inductorPath  },
        { QPointF(440, 100), 0.8, 1100, stepPath      },
    };

    for (auto& tr : traces) {
        qreal progress = fadeIn(t, tr.delayMs, tr.delayMs + 2500);
        if (progress <= 0.0) continue;

        qreal alpha = (progress < 0.5) ? progress * 0.6 : 0.15;

        p.save();
        p.translate(tr.offset);
        p.scale(tr.scale, tr.scale);

        QPen pen(QColor(0, 201, 255, static_cast<int>(255 * alpha)), 1.5);
        qreal dashLen = 300;
        pen.setDashPattern({dashLen, dashLen});
        pen.setDashOffset(dashLen * (1.0 - easeOut(progress)));
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawPath(tr.pathFn());

        p.restore();
    }
}

// 5. Node dots: small cyan circles with glow
void SplashScreen::drawNodeDots(QPainter& p, qreal t)
{
    struct NodeInfo { qreal x, y, delayMs; };
    NodeInfo nodes[] = {
        { 0.12 * SPLASH_W, 0.15 * SPLASH_H, 1500 },
        { 0.90 * SPLASH_W, 0.70 * SPLASH_H, 1800 },
        { 0.15 * SPLASH_W, 0.78 * SPLASH_H, 2000 },
        { 0.88 * SPLASH_W, 0.28 * SPLASH_H, 2200 },
        { 0.06 * SPLASH_W, 0.45 * SPLASH_H, 1600 },
        { 0.94 * SPLASH_W, 0.55 * SPLASH_H, 1900 },
    };

    for (auto& nd : nodes) {
        qreal progress = fadeIn(t, nd.delayMs, nd.delayMs + 500);
        if (progress <= 0.0) continue;

        qreal scale = (progress < 0.6) ? 1.5 * (progress / 0.6) : 1.0;
        qreal alpha = (progress < 0.6) ? progress / 0.6 : 0.6;

        // Glow
        QRadialGradient glow(nd.x, nd.y, 12 * scale);
        glow.setColorAt(0.0, QColor(0, 229, 255, static_cast<int>(80 * alpha)));
        glow.setColorAt(1.0, Qt::transparent);
        p.setBrush(glow);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(nd.x, nd.y), 12 * scale, 12 * scale);

        // Core dot
        p.setBrush(QColor(0, 229, 255, static_cast<int>(255 * alpha)));
        p.drawEllipse(QPointF(nd.x, nd.y), 3 * scale, 3 * scale);
    }
}

// 6. Icon: rounded rect with grid and waveform
void SplashScreen::drawIcon(QPainter& p, qreal t)
{
    qreal progress = easeOut(fadeIn(t, 200, 1200));
    if (progress <= 0.0) return;

    qreal scale = 0.7 + 0.3 * progress;
    qreal alpha = progress;

    p.save();
    p.setOpacity(alpha);

    // Icon is centered, 120x120
    qreal iconW = 120, iconH = 120;
    qreal cx = SPLASH_W / 2.0, cy = SPLASH_H / 2.0 - 55;
    p.translate(cx, cy);
    p.scale(scale, scale);

    // Background rounded rect
    QRectF iconRect(-iconW / 2, -iconH / 2, iconW, iconH);
    QLinearGradient bgGrad(iconRect.topLeft(), iconRect.bottomRight());
    bgGrad.setColorAt(0.0, QColor(10, 22, 40));
    bgGrad.setColorAt(1.0, QColor(13, 42, 74));
    p.setBrush(bgGrad);
    p.setPen(QPen(QColor(0, 200, 255, 38), 1));
    p.drawRoundedRect(iconRect, 24, 24);

    // Mini grid inside
    p.setPen(QPen(QColor(0, 200, 255, 13), 0.5));
    for (qreal gx = -iconW / 2 + 16; gx < iconW / 2; gx += 16) {
        p.drawLine(QPointF(gx, -iconH / 2), QPointF(gx, iconH / 2));
    }
    for (qreal gy = -iconH / 2 + 16; gy < iconH / 2; gy += 16) {
        p.drawLine(QPointF(-iconW / 2, gy), QPointF(iconW / 2, gy));
    }

    // Waveform path
    qreal waveProgress = easeOut(fadeIn(t, 800, 2300));
    if (waveProgress > 0.0) {
        QPainterPath wave;
        wave.moveTo(-45, 0);
        wave.cubicTo(-35, 0, -25, 0, -18, 0);
        wave.lineTo(-13, 0);
        wave.cubicTo(-7, 0, -3, -15, 5, -25);
        wave.cubicTo(10, -33, 14, -33, 16, -18);
        wave.cubicTo(18, -8, 19, 0, 23, 0);
        wave.cubicTo(27, 0, 27, 0, 30, 0);
        wave.cubicTo(34, 0, 37, 14, 42, 26);
        wave.cubicTo(45, 33, 47, 30, 50, 20);
        wave.cubicTo(52, 12, 53, 0, 57, 0);
        wave.lineTo(45, 0);

        QLinearGradient waveGrad(-45, 0, 57, 0);
        waveGrad.setColorAt(0.0, QColor(0, 201, 255));
        waveGrad.setColorAt(0.5, QColor(0, 229, 255));
        waveGrad.setColorAt(1.0, QColor(0, 255, 178));

        QPen wavePen(QBrush(waveGrad), 2.5, Qt::SolidLine, Qt::RoundCap);
        qreal totalLen = wave.length();
        if (waveProgress < 1.0) {
            wavePen.setDashPattern({totalLen * waveProgress, totalLen});
        }
        p.setPen(wavePen);
        p.setBrush(Qt::NoBrush);
        p.drawPath(wave);

        // Peak dot
        if (waveProgress > 0.5) {
            qreal dotAlpha = fadeIn(waveProgress, 0.5, 0.8);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 255, 229, static_cast<int>(255 * dotAlpha)));
            p.drawEllipse(QPointF(16, -18), 2.5, 2.5);
        }
    }

    p.restore();
}

// 7. "ELESIM" title with animated letter spacing
void SplashScreen::drawTitle(QPainter& p, qreal t)
{
    qreal progress = easeOut(fadeIn(t, 1200, 2200));
    if (progress <= 0.0) return;

    p.save();

    QString text = "ELESIM";
    QFont font("Segoe UI", 42);
    font.setBold(true);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 30 - 18 * progress);
    p.setFont(font);

    // Gradient brush
    QLinearGradient textGrad(0, 0, SPLASH_W, 0);
    textGrad.setColorAt(0.0, QColor(0, 201, 255));
    textGrad.setColorAt(0.5, QColor(0, 229, 255));
    textGrad.setColorAt(1.0, QColor(0, 255, 178));

    p.setOpacity(progress);
    p.setPen(QPen(QBrush(textGrad), 0));
    p.setBrush(Qt::NoBrush);

    QFontMetrics fm(font);
    int textW = fm.horizontalAdvance(text);
    int textX = (SPLASH_W - textW) / 2;
    int textY = SPLASH_H / 2 + 30;

    p.drawText(textX, textY, text);

    // Underline
    if (progress > 0.5) {
        qreal lineProgress = fadeIn(progress, 0.5, 1.0);
        qreal lineW = textW * lineProgress;
        qreal lineX = SPLASH_W / 2.0 - lineW / 2.0;
        QLinearGradient lineGrad(lineX, 0, lineX + lineW, 0);
        lineGrad.setColorAt(0.0, Qt::transparent);
        lineGrad.setColorAt(0.5, QColor(0, 229, 255, 128));
        lineGrad.setColorAt(1.0, Qt::transparent);
        p.setPen(Qt::NoPen);
        p.setBrush(lineGrad);
        p.drawRect(QRectF(lineX, textY + 4, lineW, 1));
    }

    p.restore();
}

// 8. Tagline
void SplashScreen::drawTagline(QPainter& p, qreal t)
{
    qreal progress = easeOut(fadeIn(t, 1800, 2600));
    if (progress <= 0.0) return;

    p.save();
    QFont font("Consolas", 10);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 6);
    p.setFont(font);
    p.setOpacity(progress);
    p.setPen(QColor(0, 200, 255, 128));

    QString text = "SIMULATE \u00B7 ANALYZE \u00B7 BUILD";
    QFontMetrics fm(font);
    int textW = fm.horizontalAdvance(text);
    int textX = (SPLASH_W - textW) / 2;
    int textY = SPLASH_H / 2 + 55 + static_cast<int>(10 * (1.0 - progress));

    p.drawText(textX, textY, text);
    p.restore();
}

// 9. Loading bar
void SplashScreen::drawLoadingBar(QPainter& p, qreal t)
{
    qreal barAppear = fadeIn(t, 2200, 2400);
    if (barAppear <= 0.0) return;

    p.save();
    p.setOpacity(barAppear);

    int barW = 200, barH = 2;
    int barX = (SPLASH_W - barW) / 2;
    int barY = SPLASH_H - 80;

    // Background track
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 200, 255, 25));
    p.drawRoundedRect(barX, barY, barW, barH, 1, 1);

    // Fill
    qreal fillProgress = fadeIn(t, 2500, 3300);
    // Non-linear loading curve
    qreal fill = 0;
    if (fillProgress <= 0.2) fill = fillProgress / 0.2 * 0.25;
    else if (fillProgress <= 0.5) fill = 0.25 + (fillProgress - 0.2) / 0.3 * 0.30;
    else if (fillProgress <= 0.8) fill = 0.55 + (fillProgress - 0.5) / 0.3 * 0.30;
    else fill = 0.85 + (fillProgress - 0.8) / 0.2 * 0.15;

    int fillW = static_cast<int>(barW * fill);
    if (fillW > 0) {
        QLinearGradient fillGrad(barX, barY, barX + barW, barY);
        fillGrad.setColorAt(0.0, QColor(0, 201, 255));
        fillGrad.setColorAt(1.0, QColor(0, 255, 178));
        p.setBrush(fillGrad);
        p.drawRoundedRect(barX, barY, fillW, barH, 1, 1);
    }

    // "Initializing engine" text
    qreal textAppear = fadeIn(t, 2400, 2600);
    if (textAppear > 0.0) {
        QFont font("Consolas", 8);
        font.setLetterSpacing(QFont::AbsoluteSpacing, 3);
        p.setFont(font);
        p.setOpacity(textAppear * 0.35);
        p.setPen(QColor(0, 200, 255));

        QString text = "INITIALIZING ENGINE";
        QFontMetrics fm(font);
        int textW = fm.horizontalAdvance(text);
        p.drawText((SPLASH_W - textW) / 2, barY + 18, text);
    }

    p.restore();
}

// 10. Version text
void SplashScreen::drawVersion(QPainter& p, qreal t)
{
    qreal progress = fadeIn(t, 3000, 3200);
    if (progress <= 0.0) return;

    p.save();
    QFont font("Consolas", 8);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 2);
    p.setFont(font);
    p.setOpacity(progress * 0.2);
    p.setPen(QColor(0, 200, 255));

    QString text = "v" + QApplication::applicationVersion();
    QFontMetrics fm(font);
    int textW = fm.horizontalAdvance(text);
    p.drawText((SPLASH_W - textW) / 2, SPLASH_H - 30, text);
    p.restore();
}
