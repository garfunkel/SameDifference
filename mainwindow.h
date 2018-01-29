#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

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
		SortFilterProxyModel sortProxyModel;
		bool timeToDie;
		QString addFilesDialogTitle;

	signals:
		void inputFilesListChanged();

	public slots:
		void inputFileSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
		void addFiles();
		void addDir();
		void removeFiles();
		void clearFiles();
		void showPreferences();
		void checkSimilarity();
		void updateInputFileCounter();

	private slots:
		void applyPreferences();
		void toggleShowHiddenFiles(const bool show);
};

#endif // MAINWINDOW_H
