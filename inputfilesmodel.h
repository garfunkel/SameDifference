#ifndef INPUTFILESMODEL_H
#define INPUTFILESMODEL_H

#include <QAbstractTableModel>
#include <QMutex>

QString humanReadableFileSize(const qint64 size);

enum InputFileItemStatus {
	Loading,
	Ready,
	Failed
};

class InputFileItem
{
	friend class QVector<InputFileItem>;

	public:
		static int requiredInfoPieces;

		InputFileItem(const QString path);
		QString getPath() const { return path; }
		QString getFileName() const;
		QString getMediaType() const { return mediaType; }
		QString getDuration() const { return duration; }
		qint64 getSize() const { return size; }
		QString getResolution() const { return resolution; }
		QString getCodec() const { return codec; }
		QString getContainer() const { return container; }
		InputFileItemStatus getStatus() const { return status; }
		QString getError() { return error; }
		int getVideoInfo();

		bool operator ==(const InputFileItem other) const { return path == other.path; }

		static QString getLoadingPlaceholder() { return "Loading..."; }
		static QString getErrorPlaceholder() { return "Error"; }

	private:
		QString path;
		QString mediaType;
		QString duration;
		qint64 size;
		QString resolution;
		QString codec;
		QString container;
		InputFileItemStatus status;
		QString error;
		int currentInfoPieces;

		InputFileItem() { ; }
};

class InputFilesModel: public QAbstractTableModel
{
	Q_OBJECT

	public:
		explicit InputFilesModel(QObject *parent = nullptr);

		// Header:
		QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

		// Basic functionality:
		int rowCount(const QModelIndex &parent = QModelIndex()) const override;
		int columnCount(const QModelIndex &parent = QModelIndex()) const override;
		QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

		// Model management:
		void add(const InputFileItem item);
		void remove(const QModelIndex index);
		void clear();

	private:
		QMutex removeInputFileItemsMutex;
		QMutex addInputFileItemsMutex;
		QVector<InputFileItem> inputFileItems;
};

#endif // INPUTFILESMODEL_H
