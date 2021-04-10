#ifndef XPPUTIL_H
#define XPPUTIL_H

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "util.h"
#include <QScopedPointer>

class QImage;

template <typename T>
using XDeleter = GenericDeleter<T, int, void, XFree >;

template <typename T>
class XPtrFactory {
public:
    typedef QScopedPointer< T, XDeleter<T> > ScopedPointer;
};

typedef XPtrFactory<XClassHint>::ScopedPointer XScopedPtrClassHint;
typedef XPtrFactory<Window>::ScopedPointer XScopedPtrWindow;
typedef XPtrFactory<char>::ScopedPointer  XScopedPtrChar;
typedef XPtrFactory<uchar>::ScopedPointer XScopedPtrUChar;

namespace XppUtil {
bool GetPreferredWMName(Display* d, Window win, XScopedPtrUChar &ret);
bool GetVMName(Display* d, Window win, XScopedPtrUChar &ret);
bool GetNetVMName(Display* d, Window win, XScopedPtrUChar &ret);
bool FetchName(Display* d, Window win, XScopedPtrChar &ret);
bool GetIcon(Display *d, Window win, QImage &img, long prefSize);
}

#endif // XPPUTIL_H
