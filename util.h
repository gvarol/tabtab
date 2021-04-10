#ifndef UTIL_H
#define UTIL_H

//#include <QDebug>

/*
 * T:  type to delete,
 * TR: deleter ret type,
 * TV: deleter arg type,
 * Cleanup: deleter Function (e.g. XFree)
 */

template<typename T, typename TR, typename TV, TR(*Cleanup)(TV*)>
class GenericDeleter {
public:
    static inline void cleanup(T * ptr){
        if (ptr != nullptr) {
            Cleanup(ptr);
        }
    }
};

#endif // UTIL_H
