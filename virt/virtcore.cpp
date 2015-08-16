#include "virtcore.h"
#include "vboxcore.h"

VirtCore::VirtCore(QObject *parent)
	: QObject(parent),
	volumeInformation_(),
	virtcorethread_(nullptr)
{}

VirtCore::~VirtCore()
{

}

QStringList VirtCore::machineList() {
	QStringList names;

	const auto vbox = virtualBox();
	com::SafeIfaceArray<IMachine> machines;
	if (!vbox || !vboxCheck(vbox->get_Machines(
		ComSafeArrayAsOutParam(machines)
	))) {
		emit processMessage(tr("Error: can't get the list of machines"));
		return QStringList();
	}

	for (int i = 0; i < machines.size(); ++i ) {
		const auto name = ::name(machines[i]);
		if (name.isEmpty()) {
			if(SUCCEEDED(vboxLastError())) {
				emit processMessage(tr("Warning: exists an empty name"));
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

void VirtCore::virtCoreThreadFinished() {
	const auto sender = qobject_cast<VirtCoreThread*>(this->sender());
	if (sender && sender != virtcorethread_) {
		sender->deleteLater();
		qWarning() << "invalid sender";
		return;
	}

	if (!virtcorethread_) {
		qWarning() << "null thread";
		return;
	}

	if (virtcorethread_->isFinished()) {
		if (virtcorethread_->successQuery()) {
			volumeInformation_ = virtcorethread_->volumeInformation();
			emit volumeInformationQueryFinished();
		}
	}
	else {
		virtcorethread_->stop();
		virtcorethread_->wait();
	}

	auto tmp = virtcorethread_;
	virtcorethread_ = 0;
	tmp->deleteLater();
}

void VirtCore::volumeInformationQuery(const MachineData& machine) {
	emit processMessage("Information update. Please, wait...");

	if (virtcorethread_) {
		virtCoreThreadFinished();
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
		virtcorethread_, SIGNAL(processMessage(QString)),
		this, SIGNAL(processMessage(QString)),
		Qt::QueuedConnection
	);

	QObject::connect(
		virtcorethread_, SIGNAL(finished()), 
		this, SLOT(virtCoreThreadFinished()),
		Qt::QueuedConnection
	);

	virtcorethread_->start();
}
