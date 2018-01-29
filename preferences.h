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
		static const int DEFAULT_SIMILARITY_THRESHOLD;
		static const CheckFiles DEFAULT_CHECK_FILES;

		explicit Preferences(QWidget *parent = 0);
		~Preferences();

		int getSimilarityThreshold() const;
		CheckFiles getCheckFiles() const;

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
