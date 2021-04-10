// TODO: remove all Qt headers

#include <QImage>

#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>

#include "xpputil.h"
#include <iostream>

//#define SHOW_IMG

namespace XppUtil {
static bool GetTextProp(Display* d, Window win, const char* propName, XTextProperty &xprop);
}

static void convertARGB32(long *imgdata, const ulong npixels) {
    uint32_t *u32data = reinterpret_cast<uint32_t*>(imgdata);
    for (long *p = imgdata; p < &imgdata[npixels]; p++) {
        *u32data = static_cast<uint32_t>(*p);
        u32data++;
    }
}

#ifdef SHOW_IMG
static void showImage(const long *imgdata, long width, long height) {
    // TODO: dialog is not freed / only use for debugging
    const ulong npixels = width * height;
    long *icopy = new long[npixels];
    std::copy(imgdata, imgdata + npixels, icopy);
    if (sizeof(long) == 8) {
        convertARGB32(icopy, npixels);
    }

    QImage img = QImage(reinterpret_cast<uchar*>(icopy), width, height,
                 sizeof(uint32_t) * width, QImage::Format_ARGB32,
                        [](void * ptr){ delete[] reinterpret_cast<long*>(ptr); }, icopy );

    QDialog * dialog = new QDialog(nullptr, Qt::Window);
    dialog->setWindowTitle(QString::asprintf("(%d,%d)", img.width(),img.height()));
    QVBoxLayout * vlayout = new QVBoxLayout(dialog);
    QLabel * lbl = new QLabel(dialog);
    vlayout->addWidget(lbl);
    QPixmap pmap;
    pmap.convertFromImage(img);
    lbl->setPixmap(pmap);
    lbl->setFrameShape(QFrame::Box);
    lbl->setAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
    dialog->show();
}
#endif

bool XppUtil::GetIcon(Display *d, Window win, QImage &img, long prefSize)
{
    constexpr int MAXLEN = 512000;
    unsigned long nwords, bytesAfter;
    unsigned char *data;
    int ret_format;
    Atom ret_type;
    Atom wanted = XInternAtom(d, "_NET_WM_ICON", False /*TODO: True or False?*/);
    int status = XGetWindowProperty(d, win, wanted, 0, MAXLEN/4,
                                    False, AnyPropertyType, &ret_type,
                                    &ret_format, &nwords, &bytesAfter,
                                    &data);
    if (status != Success) {
        std::cerr << "unable to get window icon" << std::endl;
        return false;
    }

    if (ret_format != 32) {
        // 32 - wordsize = sizeof(long)
        // 16 - wordsize = sizeof(short)
        // 8  - wordsize = sizeof(char)
        std::cerr << "unsupported icon format:" << ret_format << std::endl;
        return false;
    }

    constexpr int wordSize = sizeof(long);
    Q_ASSERT(wordSize == 8 || wordSize == 4);

    long *imgdata = reinterpret_cast<long*>(data);
    struct Best {
        Best() = default;
        long *ldata;
        long width;
        long height;
        ulong npixels;
        void use(long* ld, long w, long h, long n) {
            ldata = ld; width = w; height = h; npixels = n;
        }
        bool hasPreferred(long pref) {
            return width == pref || height == pref;
        }
        uchar * data() {
            return reinterpret_cast<uchar*>(ldata);
        }
    };

    Best best = {};

    for (;;) {
        const long width = (*imgdata++);
        const long height = (*imgdata++);
        std::cerr << "img(" << width << "," << height << ")" << std::endl;
        nwords -= 2;
        const ulong npixels = width * height;
        const bool cond1 = width == prefSize || height == prefSize;
        const bool cond2 = !best.hasPreferred(prefSize) && best.npixels < npixels;
        if (cond1 || cond2) {
            best.use(imgdata, width, height, npixels);
        }

#ifdef SHOW_IMG
        showImage(imgdata, width, height);
#endif

        if (nwords > npixels) {
            nwords -= npixels;
            imgdata += npixels;
        } else {
            break;
        }
    }

    if (wordSize == 8) {
        convertARGB32(best.ldata, best.npixels);
    }

    auto imgXFree = [](void *ptr) {
        XFree(ptr);
    };

    img = QImage(best.data(), best.width, best.height,
                 sizeof(uint32_t) * best.width, QImage::Format_ARGB32, imgXFree, data);
    // TODO: this only copies a single icon and does not need original data and better rendering performance.
    //img = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    return true;
}

bool XppUtil::FetchName(Display *d, Window win, XScopedPtrChar &ret)
{
    char *name;
    if (XFetchName(d, win, &name) && name != nullptr) {
        XScopedPtrChar ptr(name);
        ret.swap(ptr);
        return true;
    }
    return false;
}

bool XppUtil::GetPreferredWMName(Display *d, Window win, XScopedPtrUChar &ret) {
    XTextProperty xprop;
    if ( GetTextProp(d, win, "_NET_WM_NAME", xprop)
         || GetTextProp(d, win, "WM_NAME", xprop) ) {
        if (xprop.value != nullptr) {
            XScopedPtrUChar ptr(xprop.value);
            ret.swap(ptr);
            return true;
        }
    }

    return false;
}

bool XppUtil::GetTextProp(Display *d, Window win, const char *propName, XTextProperty &xprop) {
    Atom atomName = XInternAtom(d, propName, False /*TODO: True or False?*/);
    if (XGetTextProperty(d, win, &xprop, atomName)) {
        return xprop.value != nullptr;
    }
    return false;
}

bool XppUtil::GetVMName(Display *d, Window win, XScopedPtrUChar &ret) {
    XTextProperty xprop;
    // Same as VM_NAME prop.
    if (XGetWMName(d, win, &xprop)) {
        if (xprop.value != nullptr) {
            // TODO: check type (e.g. utf8)
            XScopedPtrUChar ptr(xprop.value);
            ret.swap(ptr);
            return true;
        }
    }
    return false;
}

bool XppUtil::GetNetVMName(Display *d, Window win, XScopedPtrUChar &ret) {
    XTextProperty xprop;
    if (GetTextProp(d, win, "_NET_WM_NAME", xprop)) {
        if (xprop.value != nullptr) {
            // TODO: check type (e.g. utf8)
            XScopedPtrUChar ptr(xprop.value);
            ret.swap(ptr);
            return true;
        }
    }
    return false;
}
