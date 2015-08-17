#ifndef V_BOX_CORE_H
#define V_BOX_CORE_H

#include <QtCore>

#include <comdef.h>
#include <atlbase.h>
#include <atlcom.h>

#include "vbox/VirtualBox.h"
#include "vbox/com/array.h"

struct CoInit {
	CoInit() { CoInitialize(0); }
	~CoInit() { CoUninitialize(); }
};

inline QString toString(const char* str) {
	return str;
}

inline QString toString(const wchar_t* str) {
	return QString::fromWCharArray(str);
}


inline CComBSTR to_bstr(const std::wstring& str) {
	return str.c_str();
}

inline CComBSTR to_bstr(const QString& str) {
	return to_bstr(str.toStdWString());
}

inline CComBSTR to_bstr(const QByteArray& str) {
	return (const char*)str;
}

// Get last error ñode (threadsafe, thread local storage)
HRESULT vboxLastError();

// Get last error string (threadsafe, thread local storage)
QString vboxLastErrorString();

// Check and report result (threadsafe, thread local storage)
bool vboxCheck(HRESULT rc);

// Check and report result macros
// Using __FILE__, __LINE__, __FUNCSIG__ macros
// (threadsafe, thread local storage)
#define VBOX_CHECK(rc) (													\
	vboxCheck(rc) ? true : (qWarning() << vboxLastErrorString(), false)		\
)


// CoCreateInstance
// return: COM-Iterface smart pointer (using move constructor)
template<class IType> inline
CComPtr<IType> vboxCreate(REFCLSID rclsid) {
	CComPtr<IType> ptr; 
	return VBOX_CHECK(ptr.CoCreateInstance(rclsid)) ? ptr : nullptr;
}

inline CComPtr<IVirtualBox> virtualBox() { 
	return vboxCreate<IVirtualBox>(CLSID_VirtualBox);
}

inline CComPtr<ISession> session() { 
	return vboxCreate<ISession>(CLSID_Session);
}

inline CComPtr<IMachine> machineByName(
	IVirtualBox* vbox, 
	const QString& vmName
) {
	CComPtr<IMachine> machine;
	return vbox && VBOX_CHECK(
		vbox->FindMachine(to_bstr(vmName), &machine)
	) ? machine : nullptr;
}

inline QString name(IMachine* machine, const QString& defValue = QString()) {
	CComBSTR name;
	return machine && VBOX_CHECK(machine->get_Name(&name))
		? toString(name) : defValue;
}

inline MachineState state(
	IMachine* machine, 
	const MachineState& defState = MachineState_Null
) {
	MachineState state = defState;
	return machine && VBOX_CHECK(machine->get_State(&state))
		? state : defState;
}

inline bool isRuning(MachineState state) {
	return state == MachineState_Running;
}

inline bool isRuning(IMachine* machine) {
	return isRuning(state(machine));
}

inline bool lock(
	IMachine* machine, 
	ISession* session, 
	LockType type = LockType_Shared
) {
	return VBOX_CHECK(machine->LockMachine(session, type));
}

inline CComPtr<IProgress> launchVM(IMachine* machine, ISession* session) {
	CComPtr<IProgress> progress;
	return machine && session && VBOX_CHECK(
		machine->LaunchVMProcess(session, CComBSTR("gui"), 0, &progress)
	) ? progress : nullptr;
}

inline CComPtr<IConsole> console(ISession* session) {
	CComPtr<IConsole> console;
	return session && VBOX_CHECK(session->get_Console(&console))
		? console : nullptr;
}

inline CComPtr<IGuest> guest(IConsole* console) {
	CComPtr<IGuest> guest;
	return console && VBOX_CHECK(console->get_Guest(&guest))
		? guest : nullptr;
}

inline CComPtr<IGuest> guest(ISession* session) {
	return guest(console(session));
}

inline CComPtr<IGuestSession> createSession(
	IGuest* guest, 
	const QString& userName,
	const QString& password
) {
	if (!guest) {
		return nullptr;
	}

	CComPtr<IGuestSession> gsession;
	return VBOX_CHECK(
		guest->CreateSession(
			to_bstr(userName), to_bstr(password),
			nullptr, nullptr, &gsession
		)
	) ? gsession : nullptr;
}

inline CComPtr<IGuestSession> createGuestSession(
	ISession* session, 
	const QString& userName, 
	const QString& password
) {
	return createSession(guest(session), userName, password);
}

inline bool waitForLogin(IGuestSession* session, quint32 timeout = 0) {
	com::SafeArray<GuestSessionWaitForFlag> flags;
	flags.push_back(GuestSessionWaitForFlag_Start);
	GuestSessionWaitResult result;
	return session && VBOX_CHECK(
		session->WaitForArray(ComSafeArrayAsInParam(flags), timeout, &result))
		&& (result == GuestSessionWaitResult_Start);
}

inline bool isCompleted(IProgress* process) {
	BOOL bCompleted = FALSE;
	return process
		&& VBOX_CHECK(process->get_Completed(&bCompleted))
		&& (bCompleted == TRUE);
}

inline ProcessStatus status(
	IProcess* process,
	const ProcessStatus& defaultStatus = ProcessStatus_Undefined
	) {
	ProcessStatus status = defaultStatus;
	return process && VBOX_CHECK(process->get_Status(&status))
		? status : defaultStatus;
}

inline bool started(IProcess* process) {
	return status(process) == ProcessStatus_Started;
}

inline QTextCodec* consoleCodec(IGuestSession*) {
	return QTextCodec::codecForName("cp866");
}

struct SyncTimer {
	QDateTime start;
	qint64 timeout;
	bool operator () () const {
		return start.msecsTo(QDateTime::currentDateTime()) < timeout;
	}
};

// Analogue gctlHandleRunCommon, 
// src/VBox/Frontends/VBoxManage/VBoxManageGuestCtrl.cpp
inline QString runCommon(
	IGuestSession* session,
	const QStringList& args
	) {
	enum { processTimeout = 120000 };
	enum { processWaitTimeout = 5000 };
	enum { processRestartTimeout = 1000 };
	enum { stdoutHandle = 1 };
	enum { readSize = 0x00010000 };
	enum { readTimeOut = 5000 };

	if (!session) {
		return nullptr;
	}

	const SyncTimer run = {
		QDateTime::currentDateTime(),
		processTimeout
	};

	QString result;

	do {
		com::SafeArray<ProcessCreateFlag> createFlags;
		createFlags.push_back(ProcessCreateFlag_WaitForStdOut);
		ComPtr<IGuestProcess> process;
		
		com::SafeArray<IN_BSTR> rawArgs; 
		for (auto& arg : args) {
			rawArgs.push_back(to_bstr(arg));
		}

		if (!VBOX_CHECK(session->ProcessCreate(
			nullptr, ComSafeArrayAsInParam(rawArgs),
			nullptr, ComSafeArrayAsInParam(createFlags),
			processTimeout,
			process.asOutParam()
		))) {
			return nullptr;
		}

		com::SafeArray<ProcessWaitForFlag> waitFlags;
		waitFlags.push_back(ProcessWaitForFlag_Start);
		waitFlags.push_back(ProcessWaitForFlag_StdOut);
		waitFlags.push_back(ProcessWaitForFlag_Terminate);

		do {
			ProcessWaitResult waitResult;
			if (!VBOX_CHECK(process->WaitForArray(
				ComSafeArrayAsInParam(waitFlags),
				processWaitTimeout,
				&waitResult
				))) {
				break;
			}

			com::SafeArray<BYTE> outputData;
			if (VBOX_CHECK(process->Read(
				stdoutHandle,
				readSize,
				readTimeOut,
				ComSafeArrayAsOutParam(outputData)
			))) {
				const auto size = outputData.size();
				if (size > 0) {
					auto stdoutString = consoleCodec(session)->toUnicode(
						(const char*)outputData.raw(), size
					);
					result += stdoutString;
				}

				if (waitResult == ProcessWaitResult_StdOut
					|| waitResult == ProcessWaitForFlag_Terminate) {
					break;
				}
			}
		} while (run());

		if (!result.isEmpty()) {
			break;
		}

		QThread::msleep(processRestartTimeout);
	} while (run());

	if (!result.isEmpty()) {
		qDebug() <<
			'\n' + args.join(" ") + 
			'\n' + result;
	}

	return result;
}

// Gets a list of the guest operating system disks
inline QStringList volumeList(IGuestSession* session) {
	auto data = runCommon(session, { "fsutil.exe", "fsinfo", "drives" });
	data.replace('\0', ' ');

	QStringList result;
	for (int end = 0; end != data.size(); ++end) {
		end = data.indexOf('\\', end);
		if (end == -1) break;
		int begin = data.lastIndexOf(' ', end);
		result.push_back( data.mid(begin + 1, end - begin) );
	}

	return result;
}

struct VolumeSize {
	int64_t size;
	int64_t freeSize;
};

inline qint64 atoll(const QByteArray& str, const qint64 defValue = 0) {
	const char* cstr = str.data();

	char c = *cstr;
	while (isspace(c)) {
		c = *++cstr;
	}

	return ( c && (isdigit(c) || c == '-' || c == '+') ) 
		? atoll(cstr) : defValue;
}

inline qint64 atoll(const QString& str, const qint64 defValue = 0) {
	return atoll(str.toLocal8Bit(), defValue);
}

inline VolumeSize volumeSize(
	IGuestSession* session, 
	const QString& volume, 
	const VolumeSize& defValue = { -1, -1 }
) {
	const auto data = runCommon(session, {
		"fsutil.exe", "volume", "diskfree", volume 
	});

	const auto split = data.split(':', QString::SkipEmptyParts);
	
	return (split.size() == 4)
		? VolumeSize{ 
			atoll(split[1], defValue.size), 
			atoll(split[2], defValue.freeSize) 
		} : defValue;
} 

#endif