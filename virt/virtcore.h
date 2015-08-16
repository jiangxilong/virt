#ifndef VIRTCORE_H
#define VIRTCORE_H

#include "virtcorethread.h"

class VirtCore : public QObject
{
	Q_OBJECT

public:
	VirtCore(QObject *parent);
	~VirtCore();

	QStringList machineList();
	
	void volumeInformationQuery(const MachineData& machine);

	VolumeInformationList volumeInformation() const {
		return volumeInformation_;
	}

signals:
	void processMessage(const QString&);
	void volumeInformationQueryFinished();

public slots:
	void virtCoreThreadFinished();


private:
	VolumeInformationList volumeInformation_;
	VirtCoreThread* virtcorethread_;
};

#endif // VIRTCORE_H
