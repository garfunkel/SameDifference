#include <QPushButton>

#include "preferences.h"
#include "ui_preferences.h"

const CheckFiles Preferences::DEFAULT_CHECK_FILES = VideosAndImages;
const int Preferences::DEFAULT_SIMILARITY_THRESHOLD = 50;

const QString Preferences::SETTING_SIMILARITY_THRESHOLD = "similarityThreshold";
const QString Preferences::SETTING_CHECK_FILES = "checkFiles";

Preferences::Preferences(QWidget *parent): QDialog(parent),	ui(new Ui::Preferences)
{
	ui->setupUi(this);

	connect(ui->buttonBox->button(QDialogButtonBox::RestoreDefaults),
			&QPushButton::clicked,
			this,
			&Preferences::restoreDefaults);

	// Load previous settings.
	cancelSettings();
}

Preferences::~Preferences()
{
	delete ui;
}

void Preferences::restoreDefaults()
{
	ui->similarityThresholdHorizontalSlider->setValue(DEFAULT_SIMILARITY_THRESHOLD);
	ui->checkFilesComboBox->setCurrentIndex(DEFAULT_CHECK_FILES);
}

void Preferences::updateSimilarityThresholdLabel(const int value)
{
	QString desc;

	if (value < 25) {
		desc = "%1 - Only a little bit similar";
	} else if (value < 50) {
		desc = "%1 - Somewhat similar";
	} else if (value < 75) {
		desc = "%1 - Quite similar";
	} else {
		desc = "%1 - Very similar";
	}

	ui->similarityThresholdLabel->setText(desc.arg(value));
}

void Preferences::applySettings()
{
	settings.setValue(SETTING_SIMILARITY_THRESHOLD, ui->similarityThresholdHorizontalSlider->value());
	settings.setValue(SETTING_CHECK_FILES, ui->checkFilesComboBox->currentIndex());
}

void Preferences::cancelSettings()
{
	ui->similarityThresholdHorizontalSlider->setValue(settings.value(SETTING_SIMILARITY_THRESHOLD, DEFAULT_SIMILARITY_THRESHOLD).toInt());
	ui->checkFilesComboBox->setCurrentIndex(settings.value(SETTING_CHECK_FILES, DEFAULT_CHECK_FILES).toInt());
}
