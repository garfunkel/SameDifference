#include <QFileInfo>
#include <QFont>
#include <QBrush>
#include <cmath>
#include <QMutexLocker>
#include <cinttypes>

#include "inputfilesmodel.h"
#include "mediautility.h"

static QString secondsToTimestamp(double seconds) {
	int64_t minutes = seconds / 60;
	int64_t hours = minutes / 60;

	seconds = fmod(seconds, 60);
	minutes = fmod(minutes, 60);

	return QString().sprintf("%" PRId64 ":%" PRId64 ":%0.3f", hours, minutes, seconds);
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

int InputFileItem::requiredInfoPieces = 7;

InputFileItem::InputFileItem(const QString path)
{
	this->path = path;
	this->size = 0;
	this->status = Loading;
	this->currentInfoPieces = 0;
}

int InputFileItem::getVideoInfo()
{
	this->size = QFileInfo(path).size();
	int ret = 0;

	MediaUtility media = MediaUtility(path.toStdString().c_str());

	if ((ret = media.open()) == 0) {
		switch (media.getMediaType()) {
			case MEDIA_TYPE_UNKNOWN:
				this->mediaType = "Unknown";
				this->duration = "N/A";
				this->resolution = "N/A";

				break;

			case MEDIA_TYPE_VIDEO:
				this->mediaType = "Video";
				this->duration = secondsToTimestamp(media.getDuration());
				this->resolution = QString().sprintf("%dx%d", media.getWidth(), media.getHeight());

				break;

			case MEDIA_TYPE_IMAGE:
				this->mediaType = "Image";
				this->duration = "N/A";
				this->resolution = QString().sprintf("%dx%d", media.getWidth(), media.getHeight());

				break;
		}

		this->codec = media.getCodec();
		this->container = media.getContainer();
		this->status = Ready;
	} else {
		this->status = Failed;
		this->error = QString().sprintf("Error reading file - will not compare for similarity: %s.", media.getError(ret));
	}

	return ret;
}

QString InputFileItem::getFileName() const
{
	return QFileInfo(path).fileName();
}

InputFilesModel::InputFilesModel(QObject *parent): QAbstractTableModel(parent)
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

	QMutexLocker removeLocker((QMutex*)&removeInputFileItemsMutex);

	if (index.row() >= inputFileItems.length())
		return QVariant();

	InputFileItem item = inputFileItems[index.row()];

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
						return item.getDuration();

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
	QMutexLocker addLocker(&addInputFileItemsMutex);

	if (!inputFileItems.contains(item)) {
		int inputFileItemsLength = inputFileItems.length();

		beginInsertRows(QModelIndex(), inputFileItemsLength, inputFileItemsLength);
		inputFileItems.append(item);
		endInsertRows();

		inputFileItems[inputFileItemsLength].getVideoInfo();

		emit dataChanged(createIndex(inputFileItemsLength, 1), createIndex(inputFileItemsLength, 5));
	}
}

void InputFilesModel::remove(const QModelIndex index)
{
	QMutexLocker removeLocker(&removeInputFileItemsMutex);

	if (index.row() < inputFileItems.length()) {
		beginRemoveRows(QModelIndex(), index.row(), index.row());
		inputFileItems.remove(index.row());
		endRemoveRows();
	}
}

void InputFilesModel::clear()
{
	QMutexLocker removeLocker(&removeInputFileItemsMutex);

	if (!inputFileItems.isEmpty()) {
		beginRemoveRows(QModelIndex(), 0, inputFileItems.length() - 1);
		inputFileItems.clear();
		endRemoveRows();
	}
}
