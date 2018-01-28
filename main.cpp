#include <QApplication>

extern "C" {
	#include <libavformat/avformat.h>
}

#include "mainwindow.h"

int main(int argc, char *argv[])
{
	av_register_all();
	av_log_set_level(AV_LOG_QUIET);

	qRegisterMetaType<QVector<int> >("QVector<int>");

	QApplication a(argc, argv);
	MainWindow w;
	w.show();

	return a.exec();
}