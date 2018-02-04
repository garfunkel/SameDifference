#include <QFileInfo>
#include <QFont>
#include <QBrush>
#include <cmath>
#include <cinttypes>
#include <QDebug>

#include <algorithm>

#include "inputfilesmodel.h"
#include "mediautility.h"

static QString secondsToTimestamp(double seconds) {
	int64_t minutes = seconds / 60;
	int64_t hours = minutes / 60;

	seconds = fmod(seconds, 60);
	minutes = fmod(minutes, 60);

	return QString().sprintf("%0.2" PRId64 ":%0.2" PRId64 ":%06.3f", hours, minutes, seconds);
}

QString humanReadableFileSize(const qint64 size)
{
	QStringList list;
	double s = size;
	list << "kB" << "MB" << "GB" << "TB";

	QStringListIterator i(list);
	QString unit("bytes");

	while(s >= 1000.0 && i.hasNext())
	 {
		unit = i.next();
		s /= 1000.0;
	}

	if (unit == "bytes") {
		return QString().setNum(size) + " " + unit;
	}

	return QString().setNum(s, 'f', 2) + " " + unit;
}

const int InputFileItem::requiredInfoPieces = 7;

InputFileItem::InputFileItem(const QString path): fingerprint(MediaUtility::FINGERPRINT_SIZE * 8)
{
	this->path = path;
	this->size = 0;
	this->duration = 0.0;
	this->width = 0;
	this->height = 0;
	this->status = Loading;
	this->currentInfoPieces = 0;
}

int InputFileItem::getInfo()
{
	this->size = QFileInfo(path).size();
	int ret = 0;

	MediaUtility media = MediaUtility(qPrintable(path));

	if ((ret = media.open()) == 0) {
		switch (media.getMediaType()) {
			case MEDIA_TYPE_UNKNOWN:
				this->mediaType = "Unknown";
				this->duration = 0;
				this->durationTimestamp = "N/A";
				this->resolution = "N/A";

				break;

			case MEDIA_TYPE_VIDEO:
				this->mediaType = "Video";
				this->duration = media.getDuration();
				this->durationTimestamp = secondsToTimestamp(media.getDuration());
				this->width = media.getWidth();
				this->height = media.getHeight();
				this->resolution = QString().sprintf("%dx%d", width, height);

				break;

			case MEDIA_TYPE_IMAGE:
				this->mediaType = "Image";
				this->duration = 0;
				this->durationTimestamp = "N/A";
				this->width = media.getWidth();
				this->height = media.getHeight();
				this->resolution = QString().sprintf("%dx%d", width, height);

				break;
		}

		this->codec = media.getCodec();
		this->container = media.getContainer();
		this->status = Ready;

		const uint8_t *mediaFingerprint = media.getFingerprint();

		if (mediaFingerprint) {
			for (int byteIndex = 0; byteIndex < MediaUtility::FINGERPRINT_SIZE; byteIndex++)
				for (int bitIndex = 0; bitIndex < 8; bitIndex++)
					fingerprint.setBit(byteIndex * 8 + bitIndex, mediaFingerprint[byteIndex] & (1 << (7 - bitIndex)));
		}
	} else {
		this->status = Failed;
		this->error = QString("Error reading file - will not compare for similarity: %1.").arg(media.getError(ret));
	}

	return ret;
}

QString InputFileItem::getFileName() const
{
	return QFileInfo(path).fileName();
}

int InputFileItem::getFingerprintDifference(const InputFileItem otherItem) const
{
	if (mediaType == "Image" || mediaType == "Video") {
		if (otherItem.getMediaType() == "Image" || otherItem.getMediaType() == "Video") {
			int diff = 0;

			for (int i = 0; i < fingerprint.size(); i++) {
				if (fingerprint[i] != otherItem.getFingerprint()[i])
					diff++;
			}

			return diff;
		}
	}

	return -1;
}

InputFilesModel::InputFilesModel(QObject *parent):
	QAbstractTableModel(parent)
{
}

QVariant InputFilesModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	Q_UNUSED(orientation)

	if (role == Qt::DisplayRole) {
		switch (section) {
			case 0:
				return "File";

			case 1:
				return "Type";

			case 2:
				return "Duration";

			case 3:
				return "Size";

			case 4:
				return "Resolution";

			case 5:
				return "Codec";

			case 6:
				return "Format";
		}
	}

	return QVariant();
}

int InputFilesModel::rowCount(const QModelIndex &parent) const
{
	if (parent.isValid())
		return 0;

	QMutexLocker lock((QMutex *)&inputFileItemsMutex);

	return inputFileItems.length();
}

int InputFilesModel::columnCount(const QModelIndex &parent) const
{
	if (parent.isValid())
		return 0;

	return InputFileItem::requiredInfoPieces;
}

QVariant InputFilesModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid())
		return QVariant();

	QMutexLocker lock((QMutex *)&inputFileItemsMutex);

	if (index.row() >= inputFileItems.length()) {
		return QVariant();
	}

	InputFileItem item = inputFileItems[index.row()];

	lock.unlock();

	// Our UserRole implemnetation is used for sorting.
	if (role == Qt::UserRole) {
		switch (index.column()) {
			case 2:
				// Can't use std::numeric_limits<double>::max() because subtraction from it
				// apparently doesn't do anything...?
				switch (item.getStatus()) {
					case Loading:
						return (double)(std::numeric_limits<int>::max() - 1);

					case Ready:
						if (item.getDurationTimestamp() == "N/A")
							return std::numeric_limits<double>::max();

						return item.getDuration();

					case Failed:
						return (double)(std::numeric_limits<int>::max() - 2);
				}

			case 3:
				switch (item.getStatus()) {
					case Loading:
						return std::numeric_limits<qint64>::max();;

					case Ready:
						return item.getSize();

					case Failed:
						return std::numeric_limits<qint64>::max() - 1;
				}

			case 4:
				switch (item.getStatus()) {
					case Loading:
						return std::numeric_limits<int>::max() - 1;

					case Ready:
						if (item.getResolution() == "N/A")
							return std::numeric_limits<int>::max();

						return item.getWidth() * item.getHeight();

					case Failed:
						return std::numeric_limits<int>::max() - 2;
				}
		}

		// No other special sorting, so lets turn this into a DisplayRole and continue.
		role = Qt::DisplayRole;
	}

	if (role == Qt::DisplayRole) {
		switch (index.column()) {
			case 0:
				return item.getFileName();

			case 1:
				switch (item.getStatus()) {
					case Loading:
						return item.getLoadingPlaceholder();

					case Ready:
						return item.getMediaType();

					case Failed:
						return item.getErrorPlaceholder();
				}

			case 2:
				switch (item.getStatus()) {
					case Loading:
						return item.getLoadingPlaceholder();

					case Ready:
						return item.getDurationTimestamp();

					case Failed:
						return item.getErrorPlaceholder();
				}

			case 3:
				switch (item.getStatus()) {
					case Loading:
						return item.getLoadingPlaceholder();

					case Ready:
						return humanReadableFileSize(item.getSize());

					case Failed:
						return item.getErrorPlaceholder();
				}

			case 4:
				switch (item.getStatus()) {
					case Loading:
						return item.getLoadingPlaceholder();

					case Ready:
						return item.getResolution();

					case Failed:
						return item.getErrorPlaceholder();
				}

			case 5:
				switch (item.getStatus()) {
					case Loading:
						return item.getLoadingPlaceholder();

					case Ready:
						return item.getCodec();

					case Failed:
						return item.getErrorPlaceholder();
				}

			case 6:
				switch (item.getStatus()) {
					case Loading:
						return item.getLoadingPlaceholder();

					case Ready:
						return item.getContainer();

					case Failed:
						return item.getErrorPlaceholder();
				}
		}
	} else if (role == Qt::FontRole) {
		if (index.column() > 0 && item.getStatus() == Loading) {
			QFont font = QFont();

			font.setItalic(true);

			return font;
		}
	} else if (role == Qt::ForegroundRole) {
		if (item.getStatus() == Failed)
			return QBrush(Qt::red);

		else if (item.getMediaType() == "Unknown")
			return QBrush(Qt::darkRed);
	} else if (role == Qt::ToolTipRole) {
		if (item.getStatus() == Failed)
			return item.getError();

		else
			return item.getPath();
	}

	return QVariant();
}

void InputFilesModel::add(const InputFileItem item)
{
	QMutexLocker lock(&inputFileItemsMutex);

	if (!inputFileItemsHash.contains(item.getPath())) {
		int index = inputFileItems.length();

		lock.unlock();

		beginInsertRows(QModelIndex(), index, index);

		lock.relock();

		inputFileItems.append(item);
		inputFileItemsHash[item.getPath()] = index;

		lock.unlock();

		endInsertRows();
	}
}

void InputFilesModel::update(const InputFileItem item)
{
	int index = 0;
	QMutexLocker lock(&inputFileItemsMutex);

	if ((index = inputFileItemsHash.value(item.getPath())) >= 0) {
		if (index >= inputFileItems.length())
			return;

		if (index == 0 && inputFileItems[0].getPath() != item.getPath())
			return;

		inputFileItems[index] = item;

		lock.unlock();

		emit dataChanged(createIndex(index, 0), createIndex(index, InputFileItem::requiredInfoPieces - 1));
	}
}

bool InputFilesModel::removeRow(int row, const QModelIndex &parent)
{
	QMutexLocker lock(&inputFileItemsMutex);

	if (row < inputFileItems.length()) {
		lock.unlock();

		beginRemoveRows(parent, row, row);

		lock.relock();

		inputFileItemsHash.take(inputFileItems.takeAt(row).getPath());

		lock.unlock();

		endRemoveRows();
	}

	return false;
}

bool InputFilesModel::removeSelection(const QModelIndexList selection)
{
	QVector<int> rows;

	foreach (QModelIndex index, selection) {
		rows.append(index.row());
	}

	std::sort(rows.begin(), rows.end(), std::greater<int>());

	foreach (int row, rows) {
		removeRow(row);
	}

	return true;
}

void InputFilesModel::clear()
{
	QMutexLocker lock(&inputFileItemsMutex);
	int len = inputFileItems.length();

	lock.unlock();

	if (len > 0) {
		beginRemoveRows(QModelIndex(), 0, len - 1);

		lock.relock();

		inputFileItems.clear();
		inputFileItemsHash.clear();

		lock.unlock();

		endRemoveRows();
	}
}

const QVector<InputFileItem> InputFilesModel::getSimilarItems(const InputFileItem item) const
{
	int diff = 0;
	QVector<InputFileItem> similarItems;

	for (int i = 0; ; i++) {
		QMutexLocker lock((QMutex *)&inputFileItemsMutex);

		if (i >= inputFileItems.length()) {
			lock.unlock();

			break;
		}

		InputFileItem otherItem = inputFileItems[i];

		lock.unlock();

		if (item.getPath() != otherItem.getPath()) {
			diff = item.getFingerprintDifference(otherItem);
		}
	}

	return similarItems;
}

