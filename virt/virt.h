#ifndef VIRT_H
#define VIRT_H

#include "virtcore.h"
#include "ui_virt.h"

class Virt : public QMainWindow
{
	Q_OBJECT

public:
	enum MachineListColumn {
		MACHINE_COLUMN,
		USERNAME_COLUMN,
		PASSWORD_COLUMN
	};

	enum VolumeInformationColumn {
		VOLUME_COLUMN,
		TOTAL_SIZE_COLUMN,
		FREE_SIZE_COLUMN,
	};

	enum {
		UPDATE_PERIOD = 3000
	};

public:
	Virt(QWidget *parent = 0);

public:
	int machineCount() const {
		return ui.machines->rowCount();
	}
	
	int currentMachineIndex() const {
		return ui.machines->currentRow();
	}

	MachineData machine(int i) const;

	MachineData currentMachine() const {
		return machine(currentMachineIndex());
	}

public slots:
	void updateMachineList();
	void updateCurrentMachineInformation();
	void setVolumeInformation(
		const QString& machine, const VolumeInformationList& info);
	void showMessage(const QString& message, int timeout = 0);

private slots:
	void passwordOrUsernameChanged(QTableWidgetItem* item);

protected:
	virtual void closeEvent(QCloseEvent*) Q_DECL_OVERRIDE;

private:
	Ui::virtClass ui;
	QSettings settings_;
	VirtCore* core_;
};

#endif // VIRT_H
