#include <QFileDialog>
#include <QDirIterator>
#include <QtConcurrent/QtConcurrentRun>
#include <QCloseEvent>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "preferences.h"
#include "inputfilesmodel.h"
#include "mediautility.h"

MainWindow::MainWindow(QWidget *parent): QMainWindow(parent), ui(new Ui::MainWindow)
{
	ui->setupUi(this);
	ui->statusBar->showMessage("0 videos to check");

	inputFilesModel = new InputFilesModel(ui->inputFilesTableView);
	timeToDie = false;
	prefs = NULL;

	ui->inputFilesTableView->setModel(inputFilesModel);

	connect(ui->inputFilesTableView->selectionModel(),
			&QItemSelectionModel::selectionChanged,	this,
			&MainWindow::inputVideoSelectionChanged);

	connect(this, &MainWindow::inputFilesAdded, [=]() {
		ui->clearPushButton->setEnabled(inputFilesModel->rowCount() > 0);
	});
}

MainWindow::~MainWindow()
{
	delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event) {
	Q_UNUSED(event)

	timeToDie = true;

	QThreadPool::globalInstance()->waitForDone();
}

bool MainWindow::checkFileExistence(const QString path) const {
	return QFileInfo(path).exists();
}

void MainWindow::inputVideoSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
	Q_UNUSED(deselected)

	ui->removePushButton->setEnabled(!selected.isEmpty());
}

void MainWindow::addVideoFiles()
{
	QString selectedFilter = tr("Video files (*.mkv *.mp4 *.avi *.ogv *.mpg *.mpeg *.3gp)");
	QStringList paths = QFileDialog::getOpenFileNames(this, tr("Select video files"), QDir::homePath(), tr("All files (*);;") + selectedFilter, &selectedFilter);

	QtConcurrent::run([=]() {
		foreach (QString path, paths) {
			if (timeToDie)
				return;

			inputFilesModel->add(path);
		}

		emit inputFilesAdded();
	});
}

void MainWindow::addVideoDir()
{
	QString dirPath = QFileDialog::getExistingDirectory(this, tr("Select folder"), QDir::homePath());

	if (!dirPath.isEmpty()) {
		QtConcurrent::run([=]() {
			QDirIterator iter(dirPath, QDirIterator::Subdirectories);

			while (iter.hasNext()) {
				if (timeToDie)
					return;

				QFileInfo info(iter.next());

				if (info.isFile()) {
					inputFilesModel->add(info.filePath());
				}
			}

			emit inputFilesAdded();
		});
	}
}

void MainWindow::removeVideoFiles()
{
	QItemSelectionModel *selection = ui->inputFilesTableView->selectionModel();

	if (selection->hasSelection()) {
		foreach (QModelIndex index, selection->selectedRows())
			inputFilesModel->remove(index);
	}

	ui->clearPushButton->setEnabled(inputFilesModel->rowCount() > 0);
}

void MainWindow::clearVideoFiles()
{
	inputFilesModel->clear();

	ui->clearPushButton->setEnabled(false);
}

void MainWindow::showPreferences()
{
	if (!prefs) {
		prefs = new Preferences(this);

		connect(prefs, &Preferences::accepted, this, &MainWindow::applyPreferences);
	}

	prefs->show();
}

void MainWindow::applyPreferences()
{

}

void MainWindow::checkSimilarity()
{
	MediaUtility media = MediaUtility("/home/simon/frame0.ppm");
	int ret = 0;

	if ((ret = media.open()) != 0) {
		throw "error";
	}
}
