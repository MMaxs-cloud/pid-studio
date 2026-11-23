# Third-party dependencies

## Qt 6

- Upstream: https://www.qt.io/ and https://code.qt.io/
- Modules used: Core, Gui, Widgets; Test is used only by the tests.
- Qt is installed separately. No Qt source files, libraries, plugins or runtime binaries are included in this repository.
- The Qt 6.6.2 headers used to verify this project identify the alternatives `LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only`. This project does not require selecting GPL for application code: the relevant Qt modules are available under LGPL v3, subject to its terms.
- Qt license information: https://doc.qt.io/qt-6/licensing.html
- LGPL obligations: https://www.qt.io/development/open-source-lgpl-obligations

This is a source repository, not a distribution of Qt binaries. If you distribute a compiled application together with Qt, you must separately satisfy the terms for the exact Qt build, its third-party components and compiler runtime: license texts and notices, corresponding library sources/source delivery, and the ability to replace or relink LGPL libraries as applicable. This file alone does not satisfy those requirements.

## Application code and charts

The charts are implemented in `src/trendplot.*` using Qt's QPainter. No third-party plotting source is bundled or linked. No separate hardware SDK is included or required by the public build.

The application author has not granted a general redistribution license for the application's own code. Qt's licenses do not automatically license that code. This notice does not assign GPL, MIT or another license to it.
