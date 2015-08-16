#ifndef VIRTCORE_H
#define VIRTCORE_H

#include "virtcorethread.h"

class VirtCore : public QObject
{
	Q_OBJECT

public:
	VirtCore(QObject *parent);

	QStringList machineList();
	void volumeInformationQuery(const MachineData& machine);

signals:
	void processMessage(const QString&);
	void volumeInformation(const QString&,const VolumeInformationList&);

private:
	QPointer<VirtCoreThread> virtcorethread_;
};

#endif // VIRTCORE_H
