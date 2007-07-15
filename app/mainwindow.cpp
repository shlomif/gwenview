/*
Gwenview: an image viewer
Copyright 2007 Aurélien Gâteau

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
#include "mainwindow.moc"

// Qt
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QListView>
#include <QTimer>
#include <QToolButton>
#include <QSplitter>
#include <QSlider>

// KDE
#include <kactioncollection.h>
#include <kaction.h>
#include <kapplication.h>
#include <kde_file.h>
#include <kdirlister.h>
#include <kfiledialog.h>
#include <kfileitem.h>
#include <kimageio.h>
#include <kio/netaccess.h>
#include <kmenubar.h>
#include <kmountpoint.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kstatusbar.h>
#include <ktogglefullscreenaction.h>
#include <ktoolbar.h>
#include <kurl.h>
#include <kurlrequester.h>
#include <kxmlguifactory.h>

// Local
#include "contextmanager.h"
#include "documentview.h"
#include "savebar.h"
#include "selectioncontextmanageritem.h"
#include "sidebar.h"
#include <lib/archiveutils.h>
#include <lib/documentfactory.h>
#include <lib/fullscreenbar.h>
#include <lib/imageviewpart.h>
#include <lib/mimetypeutils.h>
#include <lib/sorteddirmodel.h>
#include <lib/thumbnailview.h>
#include <lib/transformimageoperation.h>

namespace Gwenview {

#undef ENABLE_LOG
#undef LOG
//#define ENABLE_LOG
#ifdef ENABLE_LOG
#define LOG(x) kDebug() << k_funcinfo << x << endl
#else
#define LOG(x) ;
#endif


static bool urlIsDirectory(QWidget* parent, const KUrl& url) {
	if( url.fileName(KUrl::ObeyTrailingSlash).isEmpty()) {
		return true; // file:/somewhere/<nothing here>
	}

	// Do direct stat instead of using KIO if the file is local (faster)
	KMountPoint::List mpl = KMountPoint::currentMountPoints();
	KMountPoint::Ptr mp = mpl.findByPath( url.path() );

	if( url.isLocalFile() && !mp->probablySlow()) {
		KDE_struct_stat buff;
		if ( KDE_stat( QFile::encodeName(url.path()), &buff ) == 0 )  {
			return S_ISDIR( buff.st_mode );
		}
	}
	KIO::UDSEntry entry;
	if( KIO::NetAccess::stat( url, entry, parent)) {
		return entry.isDir();
	}
	return false;
}

struct MainWindowState {
	QAction* mActiveViewModeAction;
	bool mSideBarVisible;
};

struct MainWindow::Private {
	MainWindow* mWindow;
	QSplitter* mCentralSplitter;
	DocumentView* mDocumentView;
	QToolButton* mGoUpButton;
	KUrlRequester* mUrlRequester;
	ThumbnailView* mThumbnailView;
	QSlider* mThumbnailSlider;
	QWidget* mThumbnailViewPanel;
	SideBar* mSideBar;
	FullScreenBar* mFullScreenBar;
	SaveBar* mSaveBar;

	QActionGroup* mViewModeActionGroup;
	QAction* mBrowseAction;
	QAction* mPreviewAction;
	QAction* mViewAction;
	QAction* mGoUpAction;
	QAction* mGoToPreviousAction;
	QAction* mGoToNextAction;
	QAction* mRotateLeftAction;
	QAction* mRotateRightAction;
	QAction* mMirrorAction;
	QAction* mFlipAction;
	QAction* mToggleSideBarAction;
	KToggleFullScreenAction* mFullScreenAction;

	SortedDirModel* mDirModel;
	ContextManager* mContextManager;

	MainWindowState mStateBeforeFullScreen;

	KUrl mUrlToSelect;

	void setupWidgets() {
		QWidget* centralWidget = new QWidget(mWindow);
		mWindow->setCentralWidget(centralWidget);
		mSaveBar = new SaveBar(centralWidget);

		mCentralSplitter = new QSplitter(Qt::Horizontal, centralWidget);
		QVBoxLayout* layout = new QVBoxLayout(centralWidget);
		layout->addWidget(mSaveBar);
		layout->addWidget(mCentralSplitter);
		layout->setMargin(0);
		layout->setSpacing(0);

		setupThumbnailView(mCentralSplitter);
		mDocumentView = new DocumentView(mCentralSplitter, mWindow);
		connect(mDocumentView, SIGNAL(partChanged(KParts::Part*)),
			mWindow, SLOT(createGui(KParts::Part*)) );

		mSideBar = new SideBar(mCentralSplitter);

		connect(mSaveBar, SIGNAL(requestSave(const KUrl&)),
			mWindow, SLOT(save(const KUrl&)) );
		connect(mSaveBar, SIGNAL(goToUrl(const KUrl&)),
			mWindow, SLOT(goToUrl(const KUrl&)) );
	}

	void setupThumbnailView(QWidget* parent) {
		mThumbnailViewPanel = new QWidget(parent);

		// mThumbnailView
		mThumbnailView = new ThumbnailView(mThumbnailViewPanel);
		mThumbnailView->setModel(mDirModel);
		mThumbnailView->setThumbnailViewHelper(mDirModel);
		mThumbnailView->setSelectionMode(QAbstractItemView::ExtendedSelection);
		connect(mThumbnailView, SIGNAL(activated(const QModelIndex&)),
			mWindow, SLOT(openDirUrlFromIndex(const QModelIndex&)) );
		connect(mThumbnailView, SIGNAL(doubleClicked(const QModelIndex&)),
			mWindow, SLOT(openDirUrlFromIndex(const QModelIndex&)) );
		connect(mThumbnailView->selectionModel(), SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)),
			mWindow, SLOT(slotSelectionChanged()) );

		// mGoUpButton
		mGoUpButton = new QToolButton(mThumbnailViewPanel);
		mGoUpButton->setAutoRaise(true);

		// mUrlRequester
		mUrlRequester = new KUrlRequester(mThumbnailViewPanel);
		mUrlRequester->setMode(KFile::Directory);
		connect(mUrlRequester, SIGNAL(urlSelected(const KUrl&)),
			mWindow, SLOT(openDirUrl(const KUrl&)) );
		connect(mUrlRequester, SIGNAL(returnPressed(const QString&)),
			mWindow, SLOT(openDirUrlFromString(const QString&)) );

		// Thumbnail slider
		KStatusBar* statusBar = new KStatusBar(mThumbnailViewPanel);
		mThumbnailSlider = new QSlider(statusBar);
		mThumbnailSlider->setMaximumWidth(200);
		statusBar->addPermanentWidget(mThumbnailSlider);
		mThumbnailSlider->setMinimum(40);
		mThumbnailSlider->setMaximum(256);
		mThumbnailSlider->setValue(128);
		mThumbnailSlider->setOrientation(Qt::Horizontal);
		connect(mThumbnailSlider, SIGNAL(valueChanged(int)), mThumbnailView, SLOT(setThumbnailSize(int)) );

		// Layout
		QGridLayout* layout = new QGridLayout(mThumbnailViewPanel);
		layout->setSpacing(0);
		layout->setMargin(0);
		layout->addWidget(mGoUpButton, 0, 0);
		layout->addWidget(mUrlRequester, 0, 1);
		layout->addWidget(mThumbnailView, 1, 0, 1, 2);
		layout->addWidget(statusBar, 2, 0, 1, 2);
	}

	void setupActions() {
		KActionCollection* actionCollection = mWindow->actionCollection();

		KStandardAction::save(mWindow, SLOT(saveCurrent()), actionCollection);
		KStandardAction::saveAs(mWindow, SLOT(saveCurrentAs()), actionCollection);
		KStandardAction::quit(KApplication::kApplication(), SLOT(quit()), actionCollection);

		mBrowseAction = actionCollection->addAction("browse");
		mBrowseAction->setText(i18n("Browse"));
		mBrowseAction->setCheckable(true);
		mBrowseAction->setIcon(KIcon("view-icon"));

		mPreviewAction = actionCollection->addAction("preview");
		mPreviewAction->setText(i18n("Preview"));
		mPreviewAction->setIcon(KIcon("view-choose"));
		mPreviewAction->setCheckable(true);

		mViewAction = actionCollection->addAction("view");
		mViewAction->setText(i18n("View"));
		mViewAction->setIcon(KIcon("image"));
		mViewAction->setCheckable(true);

		mViewModeActionGroup = new QActionGroup(mWindow);
		mViewModeActionGroup->addAction(mBrowseAction);
		mViewModeActionGroup->addAction(mPreviewAction);
		mViewModeActionGroup->addAction(mViewAction);

		connect(mViewModeActionGroup, SIGNAL(triggered(QAction*)),
			mWindow, SLOT(setActiveViewModeAction(QAction*)) );

		mFullScreenAction = KStandardAction::fullScreen(mWindow, SLOT(toggleFullScreen()), mWindow, actionCollection);

		mGoToPreviousAction = actionCollection->addAction("go_to_previous");
		mGoToPreviousAction->setText(i18n("Previous"));
		mGoToPreviousAction->setIcon(KIcon("arrow-left"));
		mGoToPreviousAction->setShortcut(Qt::Key_Backspace);
		connect(mGoToPreviousAction, SIGNAL(triggered()),
			mWindow, SLOT(goToPrevious()) );

		mGoToNextAction = actionCollection->addAction("go_to_next");
		mGoToNextAction->setText(i18n("Next"));
		mGoToNextAction->setIcon(KIcon("arrow-right"));
		mGoToNextAction->setShortcut(Qt::Key_Space);
		connect(mGoToNextAction, SIGNAL(triggered()),
			mWindow, SLOT(goToNext()) );

		mGoUpAction = KStandardAction::up(mWindow, SLOT(goUp()), actionCollection);
		mGoUpButton->setDefaultAction(mGoUpAction);

		mRotateLeftAction = actionCollection->addAction("rotate_left");
		mRotateLeftAction->setText(i18n("Rotate Left"));
		mRotateLeftAction->setIcon(KIcon("object-rotate-left"));
		mRotateLeftAction->setShortcut(Qt::CTRL + Qt::Key_L);
		connect(mRotateLeftAction, SIGNAL(triggered()),
			mWindow, SLOT(rotateLeft()) );

		mRotateRightAction = actionCollection->addAction("rotate_right");
		mRotateRightAction->setText(i18n("Rotate Right"));
		mRotateRightAction->setIcon(KIcon("object-rotate-right"));
		mRotateRightAction->setShortcut(Qt::CTRL + Qt::Key_R);
		connect(mRotateRightAction, SIGNAL(triggered()),
			mWindow, SLOT(rotateRight()) );

		mMirrorAction = actionCollection->addAction("mirror");
		mMirrorAction->setText(i18n("Mirror"));
		connect(mMirrorAction, SIGNAL(triggered()),
			mWindow, SLOT(mirror()) );

		mFlipAction = actionCollection->addAction("flip");
		mFlipAction->setText(i18n("Flip"));
		connect(mFlipAction, SIGNAL(triggered()),
			mWindow, SLOT(flip()) );

		mToggleSideBarAction = actionCollection->addAction("toggle_sidebar");
		mToggleSideBarAction->setIcon(KIcon("view-sidetree"));
		connect(mToggleSideBarAction, SIGNAL(triggered()),
			mWindow, SLOT(toggleSideBar()) );
	}


	void setupContextManager() {
		mContextManager = new ContextManager(mWindow);
		mContextManager->setSideBar(mSideBar);
		AbstractContextManagerItem* item = new SelectionContextManagerItem();
		mContextManager->addItem(item);
	}


	void initDirModel() {
		KDirLister* dirLister = mDirModel->dirLister();
		QStringList mimeTypes;
		mimeTypes += MimeTypeUtils::dirMimeTypes();
		mimeTypes += MimeTypeUtils::imageMimeTypes();
		mimeTypes += MimeTypeUtils::videoMimeTypes();
		dirLister->setMimeFilter(mimeTypes);

		connect(dirLister, SIGNAL(newItems(const KFileItemList&)),
			mWindow, SLOT(slotDirListerNewItems()) );

		connect(dirLister, SIGNAL(deleteItem(KFileItem*)),
			mWindow, SLOT(updatePreviousNextActions()) );
	}

	void updateToggleSideBarAction() {
		if (mSideBar->isVisible()) {
			mToggleSideBarAction->setText(i18n("Hide Side Bar"));
		} else {
			mToggleSideBarAction->setText(i18n("Show Side Bar"));
		}
	}

	QModelIndex getRelativeIndex(int offset) {
		KUrl url = currentUrl();
		QModelIndex index = mDirModel->indexForUrl(url);
		int row = index.row() + offset;
		index = mDirModel->index(row, 0);
		if (!index.isValid()) {
			return QModelIndex();
		}

		KFileItem* item = mDirModel->itemForIndex(index);
		if (item && !ArchiveUtils::fileItemIsDirOrArchive(item)) {
			return index;
		} else {
			return QModelIndex();
		}
	}

	void goTo(int offset) {
		QModelIndex index = getRelativeIndex(offset);
		if (index.isValid()) {
			mThumbnailView->setCurrentIndex(index);
		}
	}

	void updateUrlRequester(const KUrl& url) {
		mUrlRequester->setUrl(url);
		mGoUpAction->setEnabled(url.path() != "/");
	}

	void createFullScreenBar() {
		mFullScreenBar = new FullScreenBar(mDocumentView);
		mFullScreenBar->addAction(mFullScreenAction);
		mFullScreenBar->addAction(mGoToPreviousAction);
		mFullScreenBar->addAction(mGoToNextAction);
		mFullScreenBar->resize(mFullScreenBar->sizeHint());
	}

	bool currentDocumentIsRasterImage() {
		if (mDocumentView->isVisible()) {
			// If the document view is visible, we assume we have a raster
			// image if and only if we are using the ImageViewPart. This avoids
			// having to determine the mimetype a second time.
			// FIXME: KPart code should move to DocumentView and DocumentView
			// should be able to answer whether it's showing a raster image.
			return dynamic_cast<ImageViewPart*>(mDocumentView->part()) != 0;
		} else {
			QModelIndex index = mThumbnailView->currentIndex();
			if (!index.isValid()) {
				return false;
			}
			KFileItem* item = mDirModel->itemForIndex(index);
			Q_ASSERT(item);
			return MimeTypeUtils::fileItemKind(item) == MimeTypeUtils::KIND_RASTER_IMAGE;
		}
	}

	void updateActions() {
		bool canModify = currentDocumentIsRasterImage();
		if (!mDocumentView->isVisible()) {
			// Since we only support image operations on one image for now,
			// disable actions if several images are selected and the document
			// view is not visible.
			QItemSelection selection = mThumbnailView->selectionModel()->selection();
			QModelIndexList indexList = selection.indexes();
			if (indexList.count() != 1) {
				canModify = false;
			}
		}

		KActionCollection* actionCollection = mWindow->actionCollection();
		actionCollection->action("file_save")->setEnabled(canModify);
		actionCollection->action("file_save_as")->setEnabled(canModify);
		mRotateLeftAction->setEnabled(canModify);
		mRotateRightAction->setEnabled(canModify);
		mMirrorAction->setEnabled(canModify);
		mFlipAction->setEnabled(canModify);
	}

	KUrl currentUrl() const {
		if (mDocumentView->isVisible() && mDocumentView->part()) {
			return mDocumentView->url();
		} else {
			QModelIndex index = mThumbnailView->currentIndex();
			if (!index.isValid()) {
				return KUrl();
			}
			KFileItem* item = mDirModel->itemForIndex(index);
			Q_ASSERT(item);
			return item->url();
		}
	}

	QString currentMimeType() const {
		if (mDocumentView->isVisible() && mDocumentView->part()) {
			return MimeTypeUtils::urlMimeType(mDocumentView->url());
		} else {
			QModelIndex index = mThumbnailView->currentIndex();
			if (!index.isValid()) {
				return QString();
			}
			KFileItem* item = mDirModel->itemForIndex(index);
			Q_ASSERT(item);
			return item->mimetype();
		}
	}

	void applyImageOperation(AbstractImageOperation* op) {
		// For now, we only support operations on one image
		KUrl url = currentUrl();

		Document::Ptr doc = DocumentFactory::instance()->load(url);
		doc->waitUntilLoaded();
		op->apply(doc);
	}


	void updateSaveBar() {
		mSaveBar->setCurrentUrl(currentUrl());
	}

	void selectUrlToSelect() {
		if (!mUrlToSelect.isValid()) {
			return;
		}
		QModelIndex index = mDirModel->indexForUrl(mUrlToSelect);
		if (index.isValid()) {
			mThumbnailView->setCurrentIndex(index);
			mUrlToSelect = KUrl();
			updateSaveBar();
		}
	}
};


MainWindow::MainWindow()
: KParts::MainWindow(),
d(new MainWindow::Private)
{
	d->mWindow = this;
	d->mDirModel = new SortedDirModel(this);
	d->mFullScreenBar = 0;
	d->initDirModel();
	d->setupWidgets();
	d->setupActions();
	d->setupContextManager();
	d->updateActions();
	updatePreviousNextActions();
	d->mDocumentView->setContextManager(d->mContextManager);

	createShellGUI();
	connect(DocumentFactory::instance(), SIGNAL(saved(const KUrl&)),
		SLOT(generateThumbnailForUrl(const KUrl&)) );
	connect(DocumentFactory::instance(), SIGNAL(modified(const KUrl&)),
		SLOT(generateThumbnailForUrl(const KUrl&)) );
}


void MainWindow::setInitialUrl(const KUrl& url) {
	if (urlIsDirectory(this, url)) {
		d->mPreviewAction->trigger();
		openDirUrl(url);
	} else {
		d->mViewAction->trigger();
		d->mSideBar->hide();
		openDocumentUrl(url);
	}
	d->updateToggleSideBarAction();
}


void MainWindow::setActiveViewModeAction(QAction* action) {
	bool showDocument, showThumbnail;
	if (action == d->mBrowseAction) {
		showDocument = false;
		showThumbnail = true;
	} else if (action == d->mPreviewAction) {
		showDocument = true;
		showThumbnail = true;
	} else { // image only
		showDocument = true;
		showThumbnail = false;
	}

	// Adjust splitter policy. Thumbnail should only stretch if there is no
	// document view.
	d->mCentralSplitter->setStretchFactor(0, showDocument ? 0 : 1); // thumbnail
	d->mCentralSplitter->setStretchFactor(1, 1); // image
	d->mCentralSplitter->setStretchFactor(2, 0); // sidebar

	d->mDocumentView->setVisible(showDocument);
	if (showDocument && !d->mDocumentView->part()) {
		openSelectedDocument();
	} else if (!showDocument && d->mDocumentView->part()) {
		d->mDocumentView->reset();
	}
	d->mThumbnailViewPanel->setVisible(showThumbnail);
}


void MainWindow::openDirUrlFromIndex(const QModelIndex& index) {
	if (!index.isValid()) {
		return;
	}

	KFileItem* item = d->mDirModel->itemForIndex(index);
	if (item->isDir()) {
		openDirUrl(item->url());
	}
}


void MainWindow::openSelectedDocument() {
	if (!d->mDocumentView->isVisible()) {
		return;
	}

	QItemSelection selection = d->mThumbnailView->selectionModel()->selection();
	if (selection.size() == 0) {
		return;
	}

	QModelIndex index = selection.indexes()[0];
	if (!index.isValid()) {
		return;
	}

	KFileItem* item = d->mDirModel->itemForIndex(index);
	if (!item->isDir()) {
		openDocumentUrl(item->url());
	}
}


void MainWindow::goUp() {
	KUrl url = d->mDirModel->dirLister()->url();
	url = url.upUrl();
	openDirUrl(url);
}


void MainWindow::openDirUrl(const KUrl& url) {
	if (url.equals(d->mDirModel->dirLister()->url(), KUrl::CompareWithoutTrailingSlash)) {
		return;
	}
	d->mDirModel->dirLister()->openUrl(url);
	d->updateUrlRequester(url);
	d->mDocumentView->reset();
}


void MainWindow::openDirUrlFromString(const QString& str) {
	KUrl url(str);
	openDirUrl(url);
}

void MainWindow::openDocumentUrl(const KUrl& url) {
	if (d->mDocumentView->url() == url) {
		return;
	}
	if (!d->mDocumentView->openUrl(url)) {
		return;
	}
	d->mUrlToSelect = url;
	d->selectUrlToSelect();
}

void MainWindow::slotSetStatusBarText(const QString& message) {
	d->mDocumentView->statusBar()->showMessage(message);
}

void MainWindow::toggleSideBar() {
	d->mSideBar->setVisible(!d->mSideBar->isVisible());
	if (d->mSideBar->isVisible()) {
		updateSideBar();
	}
	d->updateToggleSideBarAction();
}


void MainWindow::updateSideBar() {
	if (!d->mSideBar->isVisible()) {
		return;
	}
	QItemSelection selection = d->mThumbnailView->selectionModel()->selection();
	QModelIndexList indexList = selection.indexes();

	KFileItemList itemList;
	Q_FOREACH(QModelIndex index, indexList) {
		KFileItem* item = d->mDirModel->itemForIndex(index);
		itemList << item;
	}

	d->mContextManager->updateSideBar(itemList);
}

void MainWindow::slotPartCompleted() {
	Q_ASSERT(d->mDocumentView->part());
	KUrl url = d->mDocumentView->url();
	KUrl dirUrl = url;
	dirUrl.setFileName("");
	if (dirUrl.equals(d->mDirModel->dirLister()->url(), KUrl::CompareWithoutTrailingSlash)) {
		QModelIndex index = d->mDirModel->indexForUrl(url);
		if (index.isValid()) {
			d->mThumbnailView->selectionModel()->select(index, QItemSelectionModel::SelectCurrent);
		}
		// All is synchronized, nothing to do
		return;
	}

	d->mDirModel->dirLister()->openUrl(dirUrl);
	d->updateUrlRequester(dirUrl);
}


void MainWindow::slotSelectionChanged() {
	openSelectedDocument();
	updateSideBar();
	d->updateActions();
	updatePreviousNextActions();
	d->updateSaveBar();
}


void MainWindow::slotDirListerNewItems() {
	if (!d->mDocumentView->part()) {
		return;
	}

	QItemSelection selection = d->mThumbnailView->selectionModel()->selection();
	if (selection.size() > 0) {
		updatePreviousNextActions();
		return;
	}

	// Nothing selected in the view yet, check if there was an url waiting to
	// be selected
	d->selectUrlToSelect();
}


void MainWindow::goToPrevious() {
	d->goTo(-1);
}


void MainWindow::goToNext() {
	d->goTo(1);
}


void MainWindow::goToUrl(const KUrl& url) {
	if (d->mDocumentView->isVisible()) {
		openDocumentUrl(url);
		// No need to change thumbnail view, slotPartCompleted will do the work
		// for us
	} else {
		KUrl dirUrl = url;
		dirUrl.setFileName("");
		openDirUrl(dirUrl);
		d->mUrlToSelect = url;
		d->selectUrlToSelect();
	}
}


void MainWindow::updatePreviousNextActions() {
	QModelIndex index;
	
	index = d->getRelativeIndex(-1);
	d->mGoToPreviousAction->setEnabled(index.isValid());

	index = d->getRelativeIndex(1);
	d->mGoToNextAction->setEnabled(index.isValid());
}


void MainWindow::toggleFullScreen() {
	if (d->mFullScreenAction->isChecked()) {
		// Go full screen
		d->mStateBeforeFullScreen.mActiveViewModeAction = d->mViewModeActionGroup->checkedAction();
		d->mStateBeforeFullScreen.mSideBarVisible = d->mSideBar->isVisible();

		d->mViewAction->trigger();
		d->mSideBar->hide();

		showFullScreen();
		menuBar()->hide();
		toolBar()->hide();
		if (!d->mFullScreenBar) {
			d->createFullScreenBar();
		}
		d->mFullScreenBar->setActivated(true);
	} else {
		d->mStateBeforeFullScreen.mActiveViewModeAction->trigger();
		d->mSideBar->setVisible(d->mStateBeforeFullScreen.mSideBarVisible);

		// Back to normal
		d->mFullScreenBar->setActivated(false);
		showNormal();
		menuBar()->show();
		toolBar()->show();
	}
}


void MainWindow::saveCurrent() {
	save(d->currentUrl());
}


void MainWindow::saveCurrentAs() {
	saveAs(d->currentUrl());
}


void MainWindow::save(const KUrl& url) {
	QString mimeType = MimeTypeUtils::urlMimeType(url);
	QStringList availableMimeTypes = KImageIO::mimeTypes(KImageIO::Writing);
	if (!availableMimeTypes.contains(mimeType)) {
		KGuiItem saveUsingAnotherFormat = KStandardGuiItem::saveAs();
		saveUsingAnotherFormat.setText(i18n("Save using another format"));
		int result = KMessageBox::warningContinueCancel(
			this,
			i18n("Gwenview can't save images in '%1' format.").arg(mimeType),
			QString() /* caption */,
			saveUsingAnotherFormat
			);
		if (result == KMessageBox::Continue) {
			saveAs(url);
		}
		return;
	}
	QStringList typeList = KImageIO::typeForMime(mimeType);
	Q_ASSERT(typeList.count() > 0);

	Document::Ptr doc = DocumentFactory::instance()->load(url);
	doc->save(url, typeList[0].toAscii());
}


void MainWindow::saveAs(const KUrl& url) {
	KFileDialog dialog(url, QString(), this);
	dialog.setOperationMode(KFileDialog::Saving);

	// Init mime filter
	QString mimeType = MimeTypeUtils::urlMimeType(url);
	QStringList availableMimeTypes = KImageIO::mimeTypes(KImageIO::Writing);
	dialog.setMimeFilter(availableMimeTypes, mimeType);

	// Show dialog
	if (!dialog.exec()) {
		return;
	}

	// Start save
	mimeType = dialog.currentMimeFilter();
	QStringList typeList = KImageIO::typeForMime(mimeType);
	Q_ASSERT(typeList.count() > 0);
	Document::Ptr doc = DocumentFactory::instance()->load(url);
	doc->save(dialog.selectedUrl(), typeList[0].toAscii());
}


void MainWindow::rotateLeft() {
	TransformImageOperation op(ROT_270);
	d->applyImageOperation(&op);
}


void MainWindow::rotateRight() {
	TransformImageOperation op(ROT_90);
	d->applyImageOperation(&op);
}


void MainWindow::mirror() {
	TransformImageOperation op(HFLIP);
	d->applyImageOperation(&op);
}


void MainWindow::flip() {
	TransformImageOperation op(VFLIP);
	d->applyImageOperation(&op);
}


void MainWindow::generateThumbnailForUrl(const KUrl& url) {
	QModelIndex index = d->mDirModel->indexForUrl(url);
	if (!index.isValid()) {
		return;
	}

	KFileItem* item = d->mDirModel->itemForIndex(index);
	if (!item) {
		return;
	}

	QList<KFileItem> list;
	list << *item;
	int size = d->mThumbnailSlider->value();
	d->mDirModel->generateThumbnailsForItems(list, size);
}


bool MainWindow::queryClose() {
	QList<KUrl> list = DocumentFactory::instance()->modifiedDocumentList();
	if (list.size() == 0) {
		return true;
	}

	int answer = KMessageBox::warningContinueCancel( this,
		i18np("One image has been modified.", "%1 images have been modified.", list.size())
		+ "\n" + i18n("If you quit now, your changes will be lost."),
		QString(),
		KGuiItem(i18n("Discard Changes")));

	switch (answer) {
	case KMessageBox::Continue:
		return true;
	default: // cancel
		return false;
	}
}


} // namespace
