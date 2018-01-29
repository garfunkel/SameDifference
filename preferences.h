#ifndef PREFERENCES_H
#define PREFERENCES_H

#include <QDialog>
#include <QSettings>

namespace Ui {
	class Preferences;
}

enum CheckFiles {
	VideosAndImages,
	Videos,
	Images,
	All
};

class QSettings;

class Preferences : public QDialog
{
	Q_OBJECT

	public:
		static const CheckFiles DEFAULT_CHECK_FILES;
		static const int DEFAULT_SIMILARITY_THRESHOLD;

		explicit Preferences(QWidget *parent = 0);
		~Preferences();

		//CheckFiles getCheckFiles() const { return ; }
		//int getSimilarityThreshold() const { return ; }

	private slots:
		void restoreDefaults();
		void updateSimilarityThresholdLabel(const int value);
		void applySettings();
		void cancelSettings();

	private:
		static const QString SETTING_SIMILARITY_THRESHOLD;
		static const QString SETTING_CHECK_FILES;

		Ui::Preferences *ui;
		QSettings settings;
};

#endif // PREFERENCES_H
