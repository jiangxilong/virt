#include "virtcorethread.h"
#include "vboxcore.h"

VirtCoreThread::VirtCoreThread(const MachineData& machine, QObject *parent) : 
	QThread(parent),
	machine_(machine),
	stop_(false)
{

}

VirtCoreThread::~VirtCoreThread() { 
	stop();
	wait();
}

void VirtCoreThread::run() {
	// Implementation for command line:
	// VBoxManage startvm "Windows XP"
	// timeout 10
	// VBoxManage guestcontrol "Windows XP" --username "vbox" --password "12345" run fsutil.exe --wait-stdout -- fsinfo drives
	// VBoxManage guestcontrol "Windows XP" --username "vbox" --password "12345" run fsutil.exe --wait-stdout -- volume diskfree c:\
	// ...
	enum { waitPeriod = 120000 };
	enum { delayLaunchVM = 3000 };
	enum { delayLogin = 500 };

	const CoInit coInit;
	
	// Get machine and create session
	const auto machine = machineByName(virtualBox(), machine_.machine);
	const auto session = ::session();

	// Launch and lock machine
	if (isRuning(machine)) {
		if (!lock(machine, session)) {
			emit processMessage(
				tr("Lock machine '%1' fail").arg(machine_.machine)
			);
			return;
		}
	}
	else {
		const auto progress = launchVM(machine, session);

		emit processMessage(tr("Launch machine '%1'").arg(machine_.machine));

		if (!progress || !waitFor(
			[&progress, &machine]() {
				progress->WaitForCompletion(ticPeriod);
				return isCompleted(progress) && isRuning(machine);
			},
			waitPeriod
		)) {
			if (progress) {
				progress->Cancel();
			}

			if (!stop_) {
				emit processMessage(
					tr("Launch error for machine '%1'").arg(machine_.machine)
					);
			}

			return;
		}
		else {
			msleep(delayLaunchVM);
			if (stop_) { return; }
		}
	}

	// Login
	const auto guestSession = createGuestSession(
		session, 
		machine_.userName, 
		machine_.password
	);

	emit processMessage(
		tr("Login user '%1' on machine '%2")
		.arg(machine_.userName)
		.arg(machine_.machine)
	);

	if (!guestSession || !waitFor(
		[&guestSession, this]() {
			GuestSessionWaitResult temp;
			guestSession->WaitFor(GuestSessionWaitForFlag_Start, ticPeriod, &temp);
			auto status = ::status(guestSession);
			if (isLogin(status)) {
				return true;
			}
			if (isLoginFail(status)) {
				this->stop_ = true; // login fail
			}
			return false;
		},
		waitPeriod
	)) {
		emit processMessage(
			tr("Login failed for user '%1' on machine '%2'")
			.arg(machine_.userName)
			.arg(machine_.machine)
		);

		return;
	}
	
	
	emit processMessage(
		tr("Success login user '%1' on machine '%2")
		.arg(machine_.userName)
		.arg(machine_.machine)
	);

	msleep(delayLogin);
	if (stop_) { return; }

	// Get volume information
	emit processMessage(tr("Get the volume list"));

	const auto volumeList = ::volumeList(guestSession);
	if (volumeList.isEmpty()) {
		emit processMessage(tr("Failed to get the volume list"));
		return;
	}
	if (stop_) { return; }

	VolumeInformationList volumeInformationList;
	volumeInformationList.reserve(volumeList.size());
	for (auto& volume : volumeList) {
		emit processMessage(tr("Get information for volume '%1'").arg(volume));
		const auto volumeSize = ::volumeSize(volume);
		volumeInformationList.push_back({
			volume,
			volumeSize.size,
			volumeSize.freeSize
		});
		if (stop_) { return; }
	}
	
	emit volumeInformation(machine_.machine, volumeInformationList);
}