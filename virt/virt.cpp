#include "virt.h"
#include "tableserialize.h"


Virt::Virt(QWidget *parent)
	: QMainWindow(parent),
	settings_("virt.ini", QSettings::IniFormat),
	core_(new VirtCore(this))
{
	ui.setupUi(this);
	deserialize(ui.machines, settings_);

	QObject::connect(
		core_, SIGNAL(processMessage(QString)),
		this, SLOT(showMessage(QString))
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

MachineData Virt::machine(int i) {
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
	auto current = ui.machines->currentRow();
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
		item(ui.machines, i, USERNAME_COLUMN)->setText(login.userName);
		item(ui.machines, i, PASSWORD_COLUMN)->setText(login.password);
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
	if (row == ui.machines->currentRow()) {
		auto machine = this->machine(row);
		if (!machine.password.isEmpty()
			&& !machine.userName.isEmpty() ) {
			updateCurrentMachineInformation();
		}
	}
}

void Virt::updateCurrentMachineInformation() {
	ui.volumes->setRowCount(0);
	core_->volumeInformationQuery(machine(ui.machines->currentRow()));
}

void Virt::setVolumeInformation(const QString& machine, const VolumeInformationList& info) {

}

void Virt::closeEvent(QCloseEvent*) {
	serialize(settings_, ui.machines);
}
