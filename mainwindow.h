#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSortFilterProxyModel>

#include <inputfilesmodel.h>

namespace Ui {
	class MainWindow;
}

class InputFilesModel;
class QItemSelection;
class Preferences;

class MainWindow: public QMainWindow
{
	Q_OBJECT

	public:
		explicit MainWindow(QWidget *parent = 0);
		~MainWindow();
		void closeEvent(QCloseEvent *event) override;

	private:
		Ui::MainWindow *ui;
		Preferences *prefs;
		InputFilesModel inputFilesModel;
		QSortFilterProxyModel sortProxyModel;
		bool timeToDie;
		QString addFilesDialogTitle;

	signals:
		void fileAdded(QString path);
		void fileInfoAdded(InputFileItem item);

	public slots:
		void inputFileSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
		void addFiles();
		void addDir();
		void removeFiles();
		void clearFiles();
		void showPreferences();
		void updateInputFileCounter();

	private slots:
		void addFile(const QString path);
		void addFileInfo(const InputFileItem item);
		void applyPreferences();
		void toggleShowHiddenFiles(const bool show);
};

#endif // MAINWINDOW_H
