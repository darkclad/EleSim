#pragma once

#include <QWidget>
#include <QElapsedTimer>
#include <QPropertyAnimation>

class QTimer;

class SplashScreen : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal opacity READ windowOpacity WRITE setWindowOpacity)

public:
    explicit SplashScreen(QWidget* parent = nullptr);

    void finish(QWidget* mainWin);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void drawBackground(QPainter& p);
    void drawGrid(QPainter& p, qreal t);
    void drawScanLine(QPainter& p, qreal t);
    void drawCircuitTraces(QPainter& p, qreal t);
    void drawNodeDots(QPainter& p, qreal t);
    void drawIcon(QPainter& p, qreal t);
    void drawTitle(QPainter& p, qreal t);
    void drawTagline(QPainter& p, qreal t);
    void drawLoadingBar(QPainter& p, qreal t);
    void drawVersion(QPainter& p, qreal t);

    QElapsedTimer m_elapsed;
    QTimer* m_repaintTimer = nullptr;
    QWidget* m_mainWin = nullptr;
};
