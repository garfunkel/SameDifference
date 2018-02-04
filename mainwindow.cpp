#include <QFileDialog>
#include <QDirIterator>
#include <QtConcurrent/QtConcurrentRun>
#include <QCloseEvent>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "preferences.h"
#include "mediautility.h"

MainWindow::MainWindow(QWidget *parent): QMainWindow(parent), ui(new Ui::MainWindow)
{
	ui->setupUi(this);

	timeToDie = false;
	prefs = new Preferences(this);

	sortProxyModel.setSourceModel(&inputFilesModel);
	sortProxyModel.setDynamicSortFilter(true);
	sortProxyModel.setSortRole(Qt::UserRole);
	sortProxyModel.setFilterKeyColumn(1);

	ui->inputFilesTableView->setModel(&sortProxyModel);
	ui->inputFilesTableView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
	ui->inputFilesTableView->horizontalHeader()->setSortIndicator(0, Qt::AscendingOrder);

	connect(ui->inputFilesTableView->selectionModel(),
			&QItemSelectionModel::selectionChanged,	this,
			&MainWindow::inputFileSelectionChanged);

	connect(this, &MainWindow::fileAdded, this, &MainWindow::addFile, Qt::BlockingQueuedConnection);
	connect(this, &MainWindow::fileInfoAdded, this, &MainWindow::addFileInfo, Qt::BlockingQueuedConnection);

	connect(prefs, &Preferences::accepted, this, &MainWindow::applyPreferences);

	// Configure app with our preferences.
	applyPreferences();
}

MainWindow::~MainWindow()
{
	delete ui;

	ui = NULL;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
	Q_UNUSED(event)

	timeToDie = true;
}

void MainWindow::inputFileSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
	Q_UNUSED(deselected)

	ui->removeFilesPushButton->setEnabled(!selected.isEmpty());
}

void MainWindow::addFiles()
{
	QStringList paths = QFileDialog::getOpenFileNames(this, tr(qPrintable(addFilesDialogTitle)), QDir::homePath());

	QtConcurrent::run([=]() {
		foreach (QString path, paths) {
			if (timeToDie)
				return;

			emit fileAdded(path);
		}
	});
}

void MainWindow::addDir()
{
	QString dirPath = QFileDialog::getExistingDirectory(this, tr("Select folder"), QDir::homePath());

	if (!dirPath.isEmpty()) {
		QtConcurrent::run([=]() {
			QDirIterator iter(dirPath, QDirIterator::Subdirectories);

			while (iter.hasNext()) {
				if (timeToDie)
					return;

				QFileInfo info(iter.next());

				if (info.isFile())
					emit fileAdded(info.filePath());
			}
		});
	}
}

void MainWindow::removeFiles()
{
	QItemSelectionModel *selection = ui->inputFilesTableView->selectionModel();

	if (selection->hasSelection()) {
		QModelIndexList rows;

		foreach (QModelIndex row, selection->selectedRows()) {
			rows.append(sortProxyModel.mapToSource(row));
		}

		inputFilesModel.removeSelection(rows);

		updateInputFileCounter();
	}
}

void MainWindow::clearFiles()
{
	inputFilesModel.clear();
	updateInputFileCounter();
}

void MainWindow::showPreferences()
{
	prefs->show();
}

void MainWindow::applyPreferences()
{
	switch (prefs->getCheckFiles()) {
		case VideosAndImages:
			ui->addFilesPushButton->setText("Add Videos && Images");
			addFilesDialogTitle = "Select Videos & Images";

			break;

		case Videos:
			ui->addFilesPushButton->setText("Add Videos");
			addFilesDialogTitle = "Select Videos";

			break;

		case Images:
			ui->addFilesPushButton->setText("Add Images");
			addFilesDialogTitle = "Select Images";

			break;

		case All:
			ui->addFilesPushButton->setText("Add Files");
			addFilesDialogTitle = "Select Files";

			break;
	}

	toggleShowHiddenFiles(ui->showHiddenCheckBox->isChecked());
}

void MainWindow::updateInputFileCounter()
{
	int total = inputFilesModel.rowCount();
	int filteredTotal = sortProxyModel.rowCount();
	int loading = inputFilesModel.match(sortProxyModel.index(0, 1), Qt::DisplayRole, "Loading", -1).length();

	ui->clearFilesPushButton->setEnabled(total > 0);

	if (ui->showHiddenCheckBox->isChecked()) {
		if (loading == 0)
			ui->statusBar->showMessage(QString("%1 files loaded").arg(total));

		else
			ui->statusBar->showMessage(QString("Processing... %1 / %2 files loaded").arg(total - loading).arg(total));
	} else {
		if (loading == 0)
			ui->statusBar->showMessage(QString("%1 files loaded").arg(filteredTotal));

		else
			ui->statusBar->showMessage(QString("Processing... %1 / %2 files loaded").arg(filteredTotal).arg(total));
	}
}

void MainWindow::toggleShowHiddenFiles(const bool show)
{
	if (show) {
		sortProxyModel.setFilterFixedString(QString());
	} else {
		switch (prefs->getCheckFiles()) {
			case VideosAndImages:
				sortProxyModel.setFilterRegExp("(Video|Image)");

				break;

			case Videos:
				sortProxyModel.setFilterFixedString("Video");

				break;

			case Images:
				sortProxyModel.setFilterFixedString("Image");

				break;

			case All:
				sortProxyModel.setFilterRegExp("(Video|Image|Unknown)");

				break;
		}
	}

	updateInputFileCounter();
}

void MainWindow::addFile(const QString path)
{
	inputFilesModel.add(path);

	updateInputFileCounter();

	QtConcurrent::run([=]() {
		if (timeToDie)
			return;

		InputFileItem item(path);

		item.getInfo();

		emit fileInfoAdded(item);
	});
}

void MainWindow::addFileInfo(const InputFileItem item)
{
	inputFilesModel.update(item);

	updateInputFileCounter();
}

void MainWindow::checkSimilarity()
{

}
