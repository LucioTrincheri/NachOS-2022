/// A data structure defining a bitmap -- an array of bits each of which can
/// be either on or off.  It is also known as a bit array, bit set or bit
/// vector.
///
/// The bitmap is represented as an array of unsigned integers, on which we
/// do modulo arithmetic to find the bit we are interested in.
///
/// The data structure is parameterized with with the number of bits being
/// managed.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#ifndef NACHOS_COREMAP__HH
#define NACHOS_COREMAP__HH


#include "lib/utility.hh"
#include "filesys/open_file.hh"
#include "lib/bitmap.hh"
#include "threads/thread.hh"
#include "threads/lock.hh"

class AddressInfoEntry {
    public: 
        int vpn;
        Thread *thread;
        bool loading = false;
};

class Coremap {
public:

    /// Initialize a bitmap with `nitems` bits; all bits are cleared.
    ///
    /// * `nitems` is the number of items in the bitmap.
    Coremap(unsigned nitems);

    /// Uninitialize a bitmap.
    ~Coremap();

    /// Set the “nth” bit.
    void Mark(unsigned which); //TODO no se usa probablemente, borrar

    /// Clear the “nth” bit.
    void Clear(unsigned which);

    /// Is the “nth” bit set?
    bool Test(unsigned which) const; //TODO no se usa probablemente, borrar

    /// Return the index of a clear bit, and as a side effect, set the bit.
    ///
    /// If no bits are clear, return -1.
    int Find(int vpn);

    /// Return the number of clear bits.
    unsigned CountClear() const;

    /// Print contents of bitmap.
    void Print() const;

    AddressInfoEntry *addressInfo; 
private:

    Bitmap * bitmap;

    Lock *lock;


};

#endif
