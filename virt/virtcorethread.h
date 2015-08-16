#ifndef VIRTCORETHREAD_H
#define VIRTCORETHREAD_H

#include <QtCore>
#include <QtWidgets>

struct VolumeInformation {
	QString volume;
	quint64 size_;
	quint64 freeSize_;
};

struct MachineData {
	QString machine;
	QString userName;
	QString password;
};

typedef QList<VolumeInformation> VolumeInformationList;

class VirtCoreThread : public QThread
{
	Q_OBJECT

public:
	enum { ticPeriod = 1000 };

public:
	VirtCoreThread(const MachineData& machine, QObject *parent);
	~VirtCoreThread();

	VolumeInformationList volumeInformation() const {
		return volumeInformation_;
	}

	bool successQuery() const {
		return successQuery_;
	}

	void stop() { stop_ = true; }

	template<class Run>
	bool waitFor(const Run& run, int waitMs) {
		for (int tic = 0; tic < waitMs && !stop_; tic += ticPeriod) {
			const auto start = QDateTime::currentDateTime();

			if (run()) {
				return true;
			}

			const auto waitTic =
				ticPeriod - start.msecsTo(QDateTime::currentDateTime());

			if (waitTic > 0 && waitTic <= ticPeriod) {
				msleep(waitTic);
			}
		}

		return false;
	}

signals:
	void processMessage(const QString&);

protected:
	virtual void run() Q_DECL_OVERRIDE;

private:
	const MachineData machine_;
	bool successQuery_;
	VolumeInformationList volumeInformation_;
	volatile bool stop_;
};

#endif // VIRTCORETHREAD_H
