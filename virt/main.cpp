#include "virt.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	Virt w;
	w.show();
	return a.exec();
}
