#include "virt.h"
#include "tableserialize.h"

enum { VOLUME_COLUMN_WIDTH = 70 };

Virt::Virt(QWidget *parent)
	: QMainWindow(parent),
	settings_("virt.ini", QSettings::IniFormat),
	core_(new VirtCore(this))
{
	ui.setupUi(this);
	ui.volumes->setColumnWidth(VOLUME_COLUMN, VOLUME_COLUMN_WIDTH);

	deserialize(ui.machines, settings_);

	QObject::connect(
		core_, SIGNAL(processMessage(QString)),
		this, SLOT(showMessage(QString))
	);

	QObject::connect(
		core_, SIGNAL(volumeInformation(QString, VolumeInformationList)),
		this, SLOT(setVolumeInformation(QString, VolumeInformationList))
	);

	updateMachineList();
}

void Virt::showMessage(const QString& message, int timeout) {
	if (!message.isEmpty()) {
		qDebug() << message;
	}
	ui.statusBar->showMessage(message, timeout);
}

QString text(QTableWidget* table, int row, int column) {
	auto item = table->item(row, column);
	if (!item) {
		qWarning() << "null item";
		return QString();
	}
	return item->text();
}

MachineData Virt::machine(int i) const {
	if (i < 0 || i > ui.machines->rowCount()) {
		qWarning() << "out of range";
		return{ QString(), QString(), QString() };
	}

	return {
		text(ui.machines, i, MACHINE_COLUMN),
		text(ui.machines, i, USERNAME_COLUMN),
		text(ui.machines, i, PASSWORD_COLUMN)
	};
}

struct Login { 
	QString userName, password;
};

void Virt::updateMachineList() {
	const Login defaultLogin = { QString(), QString() };
	QSignalBlocker signalBlocker(ui.machines);

	QHash<QString, Login> old;
	auto current = currentMachineIndex();
	if (current < 0) {
		current = 0;
	}

	QString currentName;
	for (int i = 0; i < machineCount(); ++i) {
		auto machine = this->machine(i);
		old.insert(machine.machine, { machine.userName, machine.password });
		if (i == current) {
			currentName = machine.machine;
		}
	}

	auto machines = core_->machineList();
	ui.machines->setRowCount(machines.size());
	current = 0;
	for (int i = 0; i < machines.size(); ++i) {
		auto& machine = machines[i];
		auto machineItem = item(ui.machines, i, MACHINE_COLUMN);
		machineItem->setText(machine);
		machineItem->setFlags(machineItem->flags() & ~Qt::ItemIsEditable);

		const auto login = old.value(machine, defaultLogin);
		setItemText(ui.machines, i, USERNAME_COLUMN, login.userName);
		setItemText(ui.machines, i, PASSWORD_COLUMN, login.password);
		if (machine == currentName) {
			current = i;
		}
	}

	if (!machines.empty()) {
		ui.machines->selectRow(current);
	}

	signalBlocker.unblock();

	updateCurrentMachineInformation();
}

void Virt::passwordOrUsernameChanged(QTableWidgetItem* item) {
	if (item->column() <= MACHINE_COLUMN) {
		qWarning() << "item is not password or username";
		return;
	}

	const auto row = item->row();
	if (row == currentMachineIndex()) {
		auto machine = this->machine(row);
		if (!machine.password.isEmpty()
			&& !machine.userName.isEmpty() ) {
			updateCurrentMachineInformation();
		}
	}
}

void Virt::updateCurrentMachineInformation() {
	ui.volumes->setRowCount(0);
	core_->volumeInformationQuery(currentMachine());
}

QString digitDiv(QString str, const QChar& div = QChar::Space, int divdim = 3) {
	const auto sz = str.size();
	if (sz > 0) {
		for (int pos = sz; (pos -= divdim) > 0;) {
			str.insert(pos, ' ');
		}
	}
	return str;
}

void Virt::setVolumeInformation(const QString& machine, 
	const VolumeInformationList& data) {
	if (machine != currentMachine().machine) {
		return;
	}

	showMessage("");

	const auto setItemVolumeSize = [this](int row, int column, qint64 size) {
		auto item = ::item(ui.volumes, row, column);
		item->setText(
			(size != -1) ? digitDiv(QString::number(size)) : tr("No data"));
		item->setTextAlignment(Qt::AlignCenter);
	};

	ui.volumes->setRowCount(data.size());
	for (int i = 0; i < data.size(); ++i ) {
		const auto& volume = data[i];
		setItemText(ui.volumes, i, VOLUME_COLUMN, volume.volume)
			->setTextAlignment(Qt::AlignCenter);
		setItemVolumeSize(i, TOTAL_SIZE_COLUMN, volume.size);
		setItemVolumeSize(i, FREE_SIZE_COLUMN, volume.freeSize);
	}
}

void Virt::closeEvent(QCloseEvent*) {
	serialize(settings_, ui.machines);
}
