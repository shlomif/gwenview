/*
Gwenview: an image viewer
Copyright 2007-2008 Aurélien Gâteau <aurelien.gateau@free.fr>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/
// Qt
#include <QDir>
#include <QTimer>

// KDE
#include <kaboutdata.h>
#include <kactioncollection.h>
#include <kapplication.h>
#include <kcmdlineargs.h>
#include <klocale.h>
#include <kmessagebox.h>

// Local
#include <lib/imageformats/imageformats.h>
#include "mainwindow.h"

int main(int argc, char *argv[]) {
	KAboutData aboutData(
		"gwenview",        /* appname */
		0,                 /* catalogName */
		ki18n("Gwenview"), /* programName */
		"2.1");            /* version */
	aboutData.setShortDescription(ki18n("An Image Viewer"));
	aboutData.setLicense(KAboutData::License_GPL);
	aboutData.setCopyrightStatement(ki18n("Copyright 2000-2008 Aurélien Gâteau"));
	aboutData.addAuthor(
		ki18n("Aurélien Gâteau"),
		ki18n("Main developer"),
		"aurelien.gateau@free.fr");

	KCmdLineArgs::init( argc, argv, &aboutData );

	KCmdLineOptions options;
	options.add("f", ki18n("Start in fullscreen mode"));
	options.add("+[file or folder]", ki18n("A starting file or folder"));
	KCmdLineArgs::addCmdLineOptions( options );

	KUrl url;
	bool startInFullScreen = false;
	KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
	if (args->count() > 0) {
		url = args->url(0);
	}

	if (url.isValid() && args->isSet("f")) {
		startInFullScreen = true;
	}
	args->clear();

	KApplication app;

	Gwenview::ImageFormats::registerPlugins();

	Gwenview::MainWindow* window = new Gwenview::MainWindow();
	window->show();
	window->setInitialUrl(url);
	if (startInFullScreen) {
		window->actionCollection()->action("fullscreen")->trigger();
	} else {
		window->show();
	}

	return app.exec();
}
