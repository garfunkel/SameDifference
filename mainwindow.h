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
		bool checkFileExistence(const QString path) const;

	private:
		Ui::MainWindow *ui;
		Preferences *prefs;
		InputFilesModel inputFilesModel;
		SortFilterProxyModel sortProxyModel;

		bool timeToDie;

	signals:
		void inputFilesListChanged();

	public slots:
		void inputVideoSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
		void addVideoFiles();
		void addVideoDir();
		void removeVideoFiles();
		void clearVideoFiles();
		void showPreferences();
		void checkSimilarity();
		void updateInputFileCounter();

	private slots:
		void applyPreferences();
};

#endif // MAINWINDOW_H
