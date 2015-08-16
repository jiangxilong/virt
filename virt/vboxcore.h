#ifndef V_BOX_CORE_H
#define V_BOX_CORE_H

#include <QtCore>

#include <comdef.h>
#include <atlbase.h>
#include <atlcom.h>
#include <atlsafe.h>

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

// get last error ñode (threadsafe, thread local storage)
HRESULT vboxLastError();

// check and report result (threadsafe, thread local storage)
bool vboxCheck(HRESULT rc);


// CoCreateInstance
// return: COM-Iterface smart pointer (using move constructor)
template<class IType> inline
CComPtr<IType> vboxCreate(REFCLSID rclsid) {
	CComPtr<IType> ptr; 
	return vboxCheck(ptr.CoCreateInstance(rclsid)) ? ptr : nullptr;
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
	return vbox && vboxCheck(
		vbox->FindMachine(to_bstr(vmName), &machine)
	) ? machine : nullptr;
}

inline QString name(IMachine* machine, const QString& defValue = QString()) {
	CComBSTR name;
	return machine && vboxCheck(machine->get_Name(&name))
		? toString(name) : defValue;
}

inline MachineState state(
	IMachine* machine, 
	const MachineState& defState = MachineState_Null
) {
	MachineState state = defState;
	return machine && vboxCheck(machine->get_State(&state))
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
	return vboxCheck(machine->LockMachine(session, type));
}

inline CComPtr<IProgress> launchVM(IMachine* machine, ISession* session) {
	CComPtr<IProgress> progress;
	return machine && session && vboxCheck(
		machine->LaunchVMProcess(session, CComBSTR("gui"), 0, &progress)
	) ? progress : nullptr;
}

inline CComPtr<IConsole> console(ISession* session) {
	CComPtr<IConsole> console;
	return session && vboxCheck(session->get_Console(&console))
		? console : nullptr;
}

inline CComPtr<IGuest> guest(IConsole* console) {
	CComPtr<IGuest> guest;
	return console && vboxCheck(console->get_Guest(&guest))
		? guest : nullptr;
}

inline CComPtr<IGuest> guest(ISession* session) {
	return guest(console(session));
}

inline QByteArray toBase64(QString string){
	QByteArray ba;
	ba.append(string);
	return ba.toBase64();
}

inline CComPtr<IGuestSession> createSession(
	IGuest* guest, 
	const QString& userName,
	const QString& password
) {
	if (!guest) {
		return nullptr;
	}

	const auto sessionName = to_bstr(toBase64(userName));

	com::SafeIfaceArray<IGuestSession> gsessions;
	if (vboxCheck(guest->FindSession(
		sessionName, ComSafeArrayAsOutParam(gsessions)
	))) {
		if (gsessions.size() > 0) {
			return gsessions[0];
		}
	}

	CComPtr<IGuestSession> gsession;
	return vboxCheck(
		guest->CreateSession(
			to_bstr(userName), to_bstr(password),
			nullptr, sessionName, &gsession
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

inline bool isCompleted(IProgress* process) {
	BOOL bCompleted = FALSE;
	return process
		&& vboxCheck(process->get_Completed(&bCompleted))
		&& (bCompleted == TRUE);
}

inline GuestSessionStatus status(
	IGuestSession* session,
	const GuestSessionStatus& defaultStatus = GuestSessionStatus_Undefined
) {
	GuestSessionStatus status = defaultStatus;
	return (session && vboxCheck(session->get_Status(&status)))
		? status : defaultStatus;
}

inline bool isLogin(GuestSessionStatus status) {
	return status == GuestSessionStatus_Started;
}

inline bool isLoginFail(GuestSessionStatus status) {
	return status == GuestSessionStatus_Error;
}

inline bool isLogin(IGuestSession* session) {
	return isLogin(status(session));
}

inline bool isLoginFail(IGuestSession* session) {
	return isLoginFail(status(session));
}

inline ProcessStatus status(
	IProcess* process,
	const ProcessStatus& defaultStatus = ProcessStatus_Undefined
	) {
	ProcessStatus status = defaultStatus;
	return process && vboxCheck(process->get_Status(&status))
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

// Analogue gctlHandleRunCommon, src/VBox/Frontends/VBoxManage/VBoxManageGuestCtrl.cpp
inline QString runCommon(
	IGuestSession* session,
	const QStringList& args
) {
	enum { processTimeout = 120000 };
	enum { processWaitTimeout = 5000 };
	enum { processRestartTimeout = 1000 };
	const ULONG stdoutHandle = 1ul;
	const ULONG readSize = 0x00010000ul;
	const ULONG readTimeOut = 5000;

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

		if (!vboxCheck(session->ProcessCreate(
			nullptr, ComSafeArrayAsInParam(rawArgs),
			nullptr, ComSafeArrayAsInParam(createFlags),
			processTimeout,
			process.asOutParam()
		))) {
			return nullptr;
		}

		com::SafeArray<ProcessWaitForFlag>    waitFlags;
		waitFlags.push_back(ProcessWaitForFlag_Start);
		waitFlags.push_back(ProcessWaitForFlag_StdOut);
		waitFlags.push_back(ProcessWaitForFlag_Terminate);

		do {
			ProcessWaitResult waitResult;
			if (!vboxCheck(process->WaitForArray(
				ComSafeArrayAsInParam(waitFlags),
				processWaitTimeout,
				&waitResult
				))) {
				break;
			}

			com::SafeArray<BYTE> outputData;
			if (vboxCheck(process->Read(
				stdoutHandle,
				readSize,
				readTimeOut,
				ComSafeArrayAsOutParam(outputData)
			))) {
				const auto size = outputData.size();
				if (size > 0) {
					const auto stdoutString = consoleCodec(session)->toUnicode(
						(const char*)outputData.raw(), size
					);

					qDebug() << QString(stdoutString).replace('\0', ' ');

					result += stdoutString;
				}

				if (waitResult == ProcessWaitResult_StdOut
					|| waitResult == ProcessWaitResult_WaitFlagNotSupported
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

	return result;
}

// Gets a list of the guest operating system disks
inline QStringList volumeList(IGuestSession* session) {
	auto data = runCommon(session, { "fsutil.exe", "fsinfo", "drives" });


	return QStringList();
}


#endif