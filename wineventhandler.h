#ifndef WINEVENTHANDLER_H
#define WINEVENTHANDLER_H
#include <QWidget>
#include <QTabWidget>
#include <QTimer>
#include <QVector>
#include <QMainWindow>

#include <QtDebug>

class WinEventHandler : public QObject
{
    Q_OBJECT

    class Get;

public: // static
    static void addTabbedWindow(QTabWidget* tabw, void *display, WId win);
    static void setMainWindow(QMainWindow* mainWindow);

public:
    WinEventHandler(QWidget* parentTab, QTabWidget* tabWidget, void* display, WId winId);
    ~WinEventHandler();
    bool eventFilter(QObject *watched, QEvent *event) override;

private: // static
    static QTimer *timer;
    static QPointer<QMainWindow> mainWindow;
    static QVector<QPointer<WinEventHandler>> insVec;

private:
    void* dpy;
    WId targetWId;
    QTabWidget* tabWidget;
    size_t titleHash = 0;
    bool hasChildWindow = false;

    //functions
    void destroyTab();
    QWidget* parent() {
        return static_cast<QWidget*>(QObject::parent());
    }

    static void staticTick();
    void tick();

    bool checkTarget();
};

#endif // WINEVENTHANDLER_H
