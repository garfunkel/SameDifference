#ifndef INPUTFILESMODEL_H
#define INPUTFILESMODEL_H

#include <QAbstractTableModel>
#include <QBitArray>
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
		static const int requiredInfoPieces;

		InputFileItem(const QString path);
		QString getPath() const { return path; }
		QString getFileName() const;
		QString getMediaType() const { return mediaType; }
		double getDuration() const { return duration; }
		QString getDurationTimestamp() const { return durationTimestamp; }
		qint64 getSize() const { return size; }
		int getWidth() const { return width; }
		int getHeight() const { return height; }
		QString getResolution() const { return resolution; }
		QString getCodec() const { return codec; }
		QString getContainer() const { return container; }
		QBitArray getFingerprint() const { return fingerprint; }
		int getFingerprintDifference(const InputFileItem otherItem) const;
		InputFileItemStatus getStatus() const { return status; }
		QString getError() { return error; }
		int getInfo();

		bool operator ==(const InputFileItem other) const { return path == other.path; }

		static QString getLoadingPlaceholder() { return "Loading..."; }
		static QString getErrorPlaceholder() { return "Error"; }

	private:
		QString path;
		QString mediaType;
		double duration;
		QString durationTimestamp;
		qint64 size;
		int width;
		int height;
		QString resolution;
		QString codec;
		QString container;
		QBitArray fingerprint;
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
		void update(const InputFileItem item);
		bool removeRow(int row, const QModelIndex &parent = QModelIndex());
		bool removeSelection(const QModelIndexList selection);
		void clear();

		const QVector<InputFileItem> getSimilarItems(const InputFileItem item) const;

	private:
		QVector<InputFileItem> inputFileItems;
		QHash<QString, int> inputFileItemsHash;
        mutable QMutex inputFileItemsMutex;
};

#endif // INPUTFILESMODEL_H
