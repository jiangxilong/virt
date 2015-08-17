#include "virt.h"
#include <QtWidgets/QApplication>

QString functionName(QString func) {
	func = func.left(func.indexOf('(')).trimmed();
	return func.mid(func.lastIndexOf(' ')).trimmed();
}

void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
	static const QString logName("log.txt");

	static const QMap<QtMsgType, QString> typeToString{
		{ QtMsgType::QtDebugMsg, "debug" },
		{ QtMsgType::QtWarningMsg, "warning" },
		{ QtMsgType::QtCriticalMsg, "critical" },
		{ QtMsgType::QtFatalMsg, "fatal" }
	};

	static volatile bool first = true;
	if (first) { // Clear log
		first = false;
		auto old = logName + ".old";
		QFile::remove(old);
		QDir::current().rename(logName, old);
	}

	// Log line: <timestamp> <type>: <function> <message>
	QString buffer;
	QTextStream stream(&buffer);

	stream << QDateTime::currentDateTime().toString("dd.MM.yyyy hh:mm:ss")
		<< ' ' << typeToString.value(type)
		<< ": " << functionName(context.function)
		<< ' ' << msg << '\n';

	auto local = 
		QTextCodec::codecForName("cp866")->fromUnicode(buffer);

	// Write to stdout and log file
	fwrite(local, local.size(), 1, stdout);
	QFile logFile(logName);
	logFile.open(QFile::Append);
	logFile.write(local);
}


int main(int argc, char *argv[])
{
	QApplication a(argc, argv);

	qInstallMessageHandler(messageHandler);
	qDebug() << "start " << QDir::currentPath();

	QTranslator ts;
	if (ts.load("ru", ":")) {
		QApplication::installTranslator(&ts);
	}
	else {
		qWarning() << "don't load translations file";
	}

	qRegisterMetaType<VolumeInformationList>();

	Virt w;
	w.show();
	return a.exec();
}
