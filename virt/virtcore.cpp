#include "virtcore.h"
#include "vboxcore.h"

VirtCore::VirtCore(QObject *parent)
	: QObject(parent), virtcorethread_() {}

QStringList VirtCore::machineList() {
	QStringList names;

	const auto vbox = virtualBox();
	com::SafeIfaceArray<IMachine> machines;
	if (!vbox || !VBOX_CHECK(vbox->get_Machines(
		ComSafeArrayAsOutParam(machines)
	))) {
		emit processMessage(tr("Error: can't get the list of machines"));
		return QStringList();
	}

	for (int i = 0; i < machines.size(); ++i ) {
		const auto name = ::name(machines[i]);
		if (name.isEmpty()) {
			if(SUCCEEDED(vboxLastError())) {
				emit processMessage(tr("Warning: exists an empty machine name"));
			}
			else {
				emit processMessage(tr("Error: can't get the machine name"));
			}
		}
		else {
			names.push_back(name);
		}
	}

	if(names.empty()) {
		emit processMessage(tr("list of machines is empty"));
	}

	return names;
}

void VirtCore::volumeInformationQuery(const MachineData& machine) {
	emit processMessage(tr("Information update. Please, wait..."));

	if (virtcorethread_) {
		virtcorethread_->stop();
		virtcorethread_->wait();
	}

	if (machine.machine.isEmpty()) {
		emit processMessage(tr("Error: Machine name is empty"));
		return;
	}

	if (machine.userName.isEmpty() || machine.password.isEmpty()) {
		emit processMessage(
			tr("Please enter user name and password for the machine '%1'")
			.arg(machine.machine)
		);
		return;
	}

	virtcorethread_ = new VirtCoreThread(machine, this); 
	
	QObject::connect(
		virtcorethread_, SIGNAL(finished()), 
		virtcorethread_, SLOT(deleteLater())
	);
	QObject::connect(
		virtcorethread_, SIGNAL(processMessage(QString)),
		this, SIGNAL(processMessage(QString)),
		Qt::QueuedConnection
	);

	QObject::connect(
		virtcorethread_, SIGNAL(volumeInformation(QString,VolumeInformationList)),
		this, SIGNAL(volumeInformation(QString,VolumeInformationList)),
		Qt::QueuedConnection
	);

	virtcorethread_->start();
}
