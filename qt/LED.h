#include <QtCore>
#if QT_VERSION >= 0x050000
#  include <QtWidgets>
#else
#  include <QtGui>
#endif

#ifndef LED_H
#define LED_H

class LED : public QWidget
{
  Q_OBJECT

  public:
  LED(QWidget *parent = 0);
  ~LED();

private:
  bool state;
  QColor onColor, offColor;
  int width, height;

public slots:
  void setState(bool newState);
  void toggle();

 signals:
  void clicked();

 protected:
  void paintEvent(QPaintEvent *event);
  void mousePressEvent(QMouseEvent *event);

};

#endif
