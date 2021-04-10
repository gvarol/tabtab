#include "wineventhandler.h"
#include <QDebug>
#include <QEvent>
#include <QFuture>
#include <QtConcurrent/QtConcurrent>
#include <QThread>

#include <X11/Xlib.h>
#include "xpputil.h"

QTimer* WinEventHandler::timer;
QPointer<QMainWindow> WinEventHandler::mainWindow;
QVector<QPointer<WinEventHandler>> WinEventHandler::insVec;

class WinEventHandler::Get {
public:
    static inline Display* dpy (WinEventHandler *weh) {
        return static_cast<Display*>(weh->dpy);
    };
};

void WinEventHandler::addTabbedWindow(QTabWidget *tabw, void* display, WId win)
{
    QWidget * tab = new QWidget();
    tabw->addTab(tab, "new tab");
    if (timer == nullptr) {
        timer = new QTimer();
        connect(timer, &QTimer::timeout, QOverload<>::of(&WinEventHandler::staticTick));
        timer->start(500);
    }
    new WinEventHandler(tab, tabw, display, win);
}

void WinEventHandler::setMainWindow(QMainWindow *mainWindow)
{
    WinEventHandler::mainWindow = mainWindow;
}

WinEventHandler::WinEventHandler(QWidget *parentTab, QTabWidget *tabWidget, void* display, WId winId)
    : QObject(parentTab),
      dpy(display), targetWId(winId), tabWidget(tabWidget)
{
    parentTab->installEventFilter(this);
    Display * d = Get::dpy(this);
    Window wparent = static_cast<Window>(parentTab->winId());
    XMapWindow(d, targetWId);
    XFlush(d);
    XRaiseWindow(d, targetWId);
    XFlush(d);
    XSetInputFocus(d, targetWId, RevertToParent, CurrentTime);
    XFlush(d);
    QThread::msleep(300);
    hasChildWindow = XReparentWindow(d, targetWId, wparent, 0, 0);
    XFlush(d);

    // TODO: make a function?
    XResizeWindow(d, targetWId, parentTab->width(), parentTab->height());
    XFlush(d);

    insVec.append(QPointer<WinEventHandler>(this));
    QImage img;
    if (XppUtil::GetIcon(d, targetWId, img, tabWidget->iconSize().width())) {
        QPixmap pmap = QPixmap::fromImage(img);
        int tabi = tabWidget->indexOf(parentTab);
        QIcon icon = QIcon(pmap);
        tabWidget->setTabIcon(tabi, icon);
        if (mainWindow != nullptr) {
            // TODO: do it on tab change as well.
            mainWindow->setWindowIcon(icon);
        }
    }
}

WinEventHandler::~WinEventHandler()
{
    Display* d = Get::dpy(this);

    if (hasChildWindow) {
        Window root = RootWindow(d, DefaultScreen(dpy));
        qDebug() << "root:" << root;
        XReparentWindow(d, targetWId, root, 0, 0);
        XFlush(d);
    }
    int me = insVec.indexOf(this);
    insVec[me] = insVec.last();
    insVec.pop_back();
    qDebug() << "bye";
}

bool WinEventHandler::eventFilter(QObject *watched, QEvent *event)
{
    Display * d = Get::dpy(this);

    switch (event->type()) {
    case QEvent::Paint:
    case QEvent::Enter:
    case QEvent::Leave:
        checkTarget();
        break;
    case QEvent::WindowActivate:
        if (!checkTarget()) {
            break;
        }
        XSetInputFocus(d, targetWId, RevertToParent, CurrentTime);
        XFlush(d);
        break;
    case QEvent::Resize:
    {
        if (!checkTarget()) {
            break;
        }
        QWidget* tab = static_cast<QWidget*>(watched);
        XResizeWindow(d, targetWId, tab->width(), tab->height());
        XFlush(d);
        // TODO: check ret value, is flush needed?
        break;
    }
    default:
        qDebug() << event->type();
        break;
    }
    return false; // pass the event
}

void WinEventHandler::staticTick()
{
    for (auto &ins : insVec) {
        ins->tick();
    }
}

void WinEventHandler::tick()
{
    checkTarget();
}

bool WinEventHandler::checkTarget()
{
    Display * d = Get::dpy(this);

    const int tabi = tabWidget->indexOf(parent());
    if (tabi < 0) {
        qDebug() << "tab not found?";
        return false;
    }

    XScopedPtrUChar ptrname;
    if (!XppUtil::GetPreferredWMName(d, targetWId, ptrname)) {
        qDebug() << "window closed";
        hasChildWindow = false;
        parent()->removeEventFilter(this);
        tabWidget->removeTab(tabi);
        this->deleteLater();
        return false;
    }
    char * name = reinterpret_cast<char*>(ptrname.get());

    const size_t hash = qHashBits(name, strlen(name));
    if (hash != titleHash) {
        titleHash = hash;
        QString qname = QString(name);
        tabWidget->setTabText(tabi, qname);
        if (mainWindow != nullptr
                && tabi == tabWidget->currentIndex()) {
            mainWindow->setWindowTitle(qname);
        }
        qDebug() << "new title:" << name;
    }

    return true;
}
