#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <QDialog>
#include <QActionEvent>
#include <QGridLayout>
#include <QListWidget>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QScopedPointer>
#include <QMessageBox>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "util.h"
#include "xpputil.h"
#include "wineventhandler.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    Display *d = XOpenDisplay(NULL);
    if (d == nullptr) {
       qDebug() << "Cannot open display";
       QMessageBox mb(QMessageBox::Icon::Critical,
                      QStringLiteral("X11 error"),
                      QStringLiteral("Unable to open display"),
                      QMessageBox::Button::Ok, this);
       mb.show();
       mb.exec();
       exit(1);
    }
    defaultTitle = windowTitle();
    WinEventHandler::setMainWindow(this);

    connect(ui->tabWidget, &QTabWidget::currentChanged, this, [this](int idx) {
        if (idx < 0) {
            setWindowTitle(defaultTitle);
            setFocus(); // TODO: probably need X API
            return;
        }
        setWindowTitle(ui->tabWidget->tabText(idx));
    });

    // Window -> Select
    connect(ui->actionSelect, &QAction::triggered , this, [this, d](bool /*checked*/){
        QDialog dialog(this, Qt::Window);
        QGridLayout * grid = new QGridLayout(&dialog);
        QListWidget * list = new QListWidget(&dialog);
        QDialogButtonBox * bbox = new QDialogButtonBox(
                    QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        grid->addWidget(list, 0, 0);
        grid->addWidget(bbox, 1, 0);
        Window selWindow;
        connect(bbox, &QDialogButtonBox::accepted, &dialog, [&dialog, list, &selWindow]() {
            auto sel = list->selectedItems();
            if (sel.length() < 1) {
                return;
            }
            selWindow = sel.first()->data(Qt::UserRole).toULongLong();
            dialog.accept();
        });
        connect(bbox, &QDialogButtonBox::rejected, &dialog, [&dialog](){ dialog.reject(); });
        dialog.show();

        const int s = DefaultScreen(d);
        traverse(list, d, RootWindow(d, s), 0);

        dialog.setModal(true);
        if (dialog.exec() == QDialog::Accepted) {
            WinEventHandler::addTabbedWindow(ui->tabWidget, d, selWindow);
            qDebug() << "selected:" << selWindow;
        }
    });

}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::traverse(QListWidget *list, void *_d, unsigned long rootWindow, int depth)
{
    Window root = rootWindow;
    Window ret_root;
    Window ret_parent;
    Window * ret_child;
    unsigned int nchild;

    Display * d = static_cast<Display*>(_d);
    XQueryTree(d, root, &ret_root, &ret_parent, &ret_child, &nchild);
    XScopedPtrWindow childPtr(ret_child);

    QString label;
    QColor oddcolor = QWidget::palette().color(QWidget::backgroundRole()).darker(120);

    label.reserve(64);

    for (unsigned int i = 0; i < nchild; i++) {
        int nitems = list->count();
        bool hasInfo = false;
        label.resize(depth, '-');

        Window child = ret_child[i];

        label.append(QString::asprintf("| 0x%lx |", child));

        XScopedPtrUChar vmname;
        if (XppUtil::GetPreferredWMName(d, child, vmname)) {
            label.append(QString::asprintf(" '%s'", vmname.get()));
        }

        XScopedPtrChar childName;
        if (XppUtil::FetchName(d, child, childName)) {
            label.append(QString::asprintf(" [n: %s]", childName.get()));
            hasInfo = true;
        }

        XScopedPtrClassHint hint(XAllocClassHint());
        if (!hint.isNull() && XGetClassHint(d, child, hint.get())) {
            auto h = hint.get();
            label.append(QString::asprintf(" [c: %s/%s]", h->res_name, h->res_class));
            hasInfo = true;
        }

        if (hasInfo) {
            auto wi = new QListWidgetItem(label, list);
            wi->setData(Qt::UserRole, QVariant::fromValue(child));
            if (nitems & 1) {
                wi->setBackgroundColor(oddcolor);
            }
        } else {
            traverse(list, _d, child, depth+1);
        }
    }
}
