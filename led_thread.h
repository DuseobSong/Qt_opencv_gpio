#ifndef LED_THREAD_H
#define LED_THREAD_H

#include <QThread>

class LED_thread : public QObject
{
    Q_OBJECT
public:
    explicit LED_thread(QObject *parent = nullptr);
    virutal ~LED_thread();

public slots:
    void blink(void);

signals:
    void start(void);
    void
};

#endif // LED_THREAD_H
