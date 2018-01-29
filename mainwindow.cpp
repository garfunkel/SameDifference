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

	inputFilesModel = new InputFilesModel(ui->inputFilesTableView);
	timeToDie = false;
	prefs = NULL;

	ui->inputFilesTableView->setModel(inputFilesModel);
	ui->statusBar->showMessage(QString("%1 videos").arg(inputFilesModel->rowCount()));

	connect(ui->inputFilesTableView->selectionModel(),
			&QItemSelectionModel::selectionChanged,	this,
			&MainWindow::inputVideoSelectionChanged);

	connect(this, &MainWindow::inputFilesListChanged, this,
			&MainWindow::updateInputFileCounter);
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
	QStringList paths = QFileDialog::getOpenFileNames(this, tr("Select video files"), QDir::homePath());

	QtConcurrent::run([=]() {
		foreach (QString path, paths) {
			if (timeToDie)
				return;

			inputFilesModel->add(path);

			emit inputFilesListChanged();
		}
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

					emit inputFilesListChanged();
				}
			}
		});
	}
}

void MainWindow::removeVideoFiles()
{
	QItemSelectionModel *selection = ui->inputFilesTableView->selectionModel();

	if (selection->hasSelection()) {
		foreach (QModelIndex index, selection->selectedRows()) {
			inputFilesModel->remove(index);

			emit inputFilesListChanged();
		}
	}
}

void MainWindow::clearVideoFiles()
{
	inputFilesModel->clear();

	emit inputFilesListChanged();
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

void MainWindow::updateInputFileCounter() {
	ui->clearPushButton->setEnabled(inputFilesModel->rowCount() > 0);
	ui->statusBar->showMessage(QString("%1 videos").arg(inputFilesModel->rowCount()));
}

void MainWindow::checkSimilarity()
{
	MediaUtility media = MediaUtility("/home/simon/frame0.ppm");
	int ret = 0;

	if ((ret = media.open()) != 0) {
		throw "error";
	}
}
