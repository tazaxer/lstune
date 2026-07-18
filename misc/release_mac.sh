#!/bin/sh
# Build a ppc/x86 universal binary for Mac OS X 10.4+ using Qt 4.6-4.8 Carbon.
# Requires: Xcode 3.1.x, MacOSX10.4u SDK, Qt Carbon in PATH.
qmake -spec macx-g++ \
    "CONFIG+=ppc x86" \
    QMAKE_MAC_SDK=/Developer/SDKs/MacOSX10.4u.sdk \
    QMAKE_MACOSX_DEPLOYMENT_TARGET=10.4

make -f Makefile.Release clean
make -f Makefile.Release -j4

macdeployqt lstune.app -dmg
exit 0

executable=lstune.app/Contents/MacOs/lstune
destdir=lstune.app/Contents/Framework
mkdir $destdir

# otool -L lstune.app/Contents/MacOS/lstune

#cp -Rv /Library/Frameworks/QtCore.framework $destdir
#cp -Rv /Library/Frameworks/QtGui.framework $destdir
#cp -Rv /Library/Frameworks/QtMultimedia.framework $destdir

cp -Rv /Library/Frameworks/QtCore.framework/Versions/4/QtCore $destdir
cp -Rv /Library/Frameworks/QtGui.framework/Versions/4/QtGui $destdir
cp -Rv /Library/Frameworks/QtMultimedia.framework/Versions/4/QtMultimedia $destdir

install_name_tool -id @executable_path/../Frameworks/QtCore \
    $destdir/QtCore.framework/Versions/4.0/QtCore
install_name_tool -id @executable_path/../Frameworks/QtGui \
    $destdir/QtGui.framework/Versions/4.0/QtGui
install_name_tool -id @executable_path/../Frameworks/QtMultimedia.framework/Versions/4/QtMultimedia \
    $destdir/QtMultimedia.framework/Versions/4/QtMultimedia

install_name_tool -change QtCore.framework/Versions/4/QtCore \
    @executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore \
    $executable

install_name_tool -change QtGui.framework/Versions/4/QtGui \
    @executable_path/../Frameworks/QtGui.framework/Versions/4/QtGui \
    $executable

install_name_tool -change QtMultimedia.framework/Versions/4/QtMultimedia \
    @executable_path/../Frameworks/QtMultimedia.framework/Versions/4/QtMultimedia \
    $executable


#ls /Library/Frameworks/QtMultimedia.framework/Versions/4/QtMultimedia
#ls /Library/Frameworks/QtGui.framework/Versions/4/QtGui
#ls /Library/Frameworks/QtCore.framework/Versions/4/QtCore

