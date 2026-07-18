# Building LSTune for PowerPC (Mac OS X 10.4+)

This document covers building a **ppc/x86 universal binary** of LSTune
that runs on PowerPC G4, G5, and Intel Macs under Mac OS X 10.4 (Tiger)
and later.

---

## Required toolchain

| Component | Required version | Notes |
|---|---|---|
| **Xcode** | 3.1.4 | Last Xcode to ship Apple GCC 4.0/4.2 with PowerPC support. Available from Apple Developer Downloads. |
| **Qt** | 4.6, 4.7, or 4.8 — **Carbon edition** | Must be the Carbon variant, **not** the Cocoa variant. Qt 4.x Cocoa requires 10.5+; Carbon supports 10.4. |
| **Mac OS X SDK** | `MacOSX10.4u.sdk` | Ships with Xcode 3.1.x at `/Developer/SDKs/MacOSX10.4u.sdk`. The `u` stands for "universal" (ppc + i386). |
| **Deployment target** | 10.4 | Set via `QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.4`. |

> **Why Carbon Qt and not Cocoa Qt?**  
> Qt 4's Cocoa back-end requires `NSApplication`, which was only
> available starting with Mac OS X 10.5 (Leopard). The Carbon back-end
> works on both 10.4 and 10.5, and is the correct choice for any binary
> that must run on Tiger.

---

## Architecture baseline

The build is configured for **generic PowerPC** (`-arch ppc`) with no
G4- or G5-specific tuning flags. This means:

- No `-mcpu=7450` (G4 / AltiVec)
- No `-mcpu=970` (G5 64-bit)
- No `-mpowerpc64`

The resulting binary runs on both G4 and G5 hardware.

---

## Build steps

### 1 — Environment

Open a Terminal in Xcode 3.1.4 (or set `PATH` so that the Qt 4 Carbon
`qmake` is found first):

```sh
export PATH=/usr/local/Trolltech/Qt-4.8.7/bin:$PATH   # adjust as needed
qmake --version   # should report Qt 4.x
```

### 2 — Configure and build (debug + release)

```sh
cd lstune/qt
qmake -spec macx-g++ \
    "CONFIG+=ppc x86" \
    QMAKE_MAC_SDK=/Developer/SDKs/MacOSX10.4u.sdk \
    QMAKE_MACOSX_DEPLOYMENT_TARGET=10.4
make -f Makefile.Release
```

Alternatively, simply run the provided release script from inside the
`qt/` directory:

```sh
cd lstune/qt
sh ../misc/release_mac.sh
```

### 3 — Verify universal binary

```sh
file lstune.app/Contents/MacOS/lstune
# Expected output contains both:
#   ppc:   Mach-O executable ppc
#   i386:  Mach-O executable i386

lipo -info lstune.app/Contents/MacOS/lstune
# Expected: Architectures in the fat file: lstune are: ppc i386
```

### 4 — Package as DMG

```sh
macdeployqt lstune.app -dmg
```

This bundles the Qt frameworks and produces `lstune.dmg`.

---

## Building the tests

```sh
cd lstune/tests
qmake -spec macx-g++ \
    "CONFIG+=ppc x86" \
    QMAKE_MAC_SDK=/Developer/SDKs/MacOSX10.4u.sdk \
    QMAKE_MACOSX_DEPLOYMENT_TARGET=10.4
make
./notes_test
```

---

## Portability notes added to source

| File | Change | Reason |
|---|---|---|
| `qt/lstune.pro` | `CONFIG += ppc x86`, SDK, deployment target inside `macx { }` | Enables universal binary; leaves win32/unix blocks untouched |
| `tests/tests.pro` | Same `macx { }` block | Consistent with main project |
| `qt/AudioIO.cc` | `setByteOrder()` now uses `QSysInfo::ByteOrder` | PowerPC is big-endian; hardcoded `LittleEndian` produced byte-swapped audio samples |
| `qt/AudioIO.cc` | `QVarLengthArray<qint32>` replaces C99 VLA | C99 VLAs are a GCC extension in C++ mode; `QVarLengthArray` is C++98 and Qt4-native |
| `qt/AudioIO.cc` | `and` → `&&` | Avoids reliance on a C++ alternative token |
| `qt/main.cc`, `tuner.cc`, `wheel.cc`, `wheel.h`, `LED.cc`, `LED.h` | `#include <QtWidgets>` guarded for Qt4/Qt5 | `<QtWidgets>` is Qt 5 only; Qt 4 uses `<QtGui>` |
